#pragma once
/*
 * acorn_atom_system.h — Acorn Atom system declaration
 *
 * Acorn Atom (1980) — simple 6502-based British home computer.
 *   CPU:    MOS 6502 @ 1 MHz
 *   Video:  Motorola MC6847 VDG (text 32×16, graphics 256×192)
 *   I/O:    Intel 8255 PPI (keyboard matrix scanning)
 *   I/O:    MOS 6522 VIA (cassette, printer, timers)
 *   Memory: 2KB–12KB RAM, 8KB BASIC ROM, 2KB FP ROM, 4KB OS ROM
 *   Video RAM at $8000–$97FF (6KB)
 *
 * Memory map:
 *   $0000–$09FF : Zero page, stack, user RAM (2KB base)
 *   $0A00–$7FFF : Expansion RAM (optional, up to ~30KB)
 *   $8000–$97FF : Video RAM (6KB, directly accessed by MC6847)
 *   $9800–$9FFF : Unused / optional
 *   $A000–$AFFF : Utility ROM area
 *   $B000–$B003 : Intel 8255 PPI
 *   $B800–$B80F : MOS 6522 VIA
 *   $C000–$CFFF : Atom BASIC ROM
 *   $D000–$D7FF : Floating point ROM
 *   $E000–$EFFF : ROM extension area
 *   $F000–$FFFF : OS ROM (Atom monitor)
 */


#include "systems/acorn_atom/acorn_atom_constants.hpp"
#include "core/system.hpp"
#include "core/system_lines.hpp"
#include "core/board.hpp"
#include "core/system_chip_visitors.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/video/mc6847/mc6847.hpp"
#include "chip/io/i8255.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>

// Default bus state for the Atom — inherited from MOS 6502 defaults.
#define ATOM_BUS_DEFAULT_STATE (MOS6502::default_bus_state())


// =============================================================================
// Acorn Atom chip declaration — single source of truth
// =============================================================================
//
// Row: V(ctx, type, chip, base, size, mask, overlay, label, rom_files)
//
// PPI and VIA are MMIO-only (sub-page decode via addr_mask).
// FP ROM is optional (loaded if available).
//

#define ATOM_FOR_EACH_SYSTEM_CHIP(V, ctx)                                                                    \
    V(ctx, MOS6502,    cpu,    0x0000,     0,      0, 0, "MOS 6502",   nullptr)                              \
    V(ctx, RAMChip,    ram,    0x0000, 32768,      0, 0, "RAM",        nullptr)                              \
    V(ctx, RAMChip,    vram,   0x8000,  8192,      0, 0, "Video RAM",  nullptr)                              \
    V(ctx, i8255_t,    ppi,    0xB000,     0, 0xFFFC, 0, "i8255 PPI",  nullptr)                              \
    V(ctx, mos6522_t,  via,    0xB800,     0, 0xFFF0, 0, "VIA 6522",   nullptr)                              \
    V(ctx, ROMChip,    basic,  0xC000,  4096,      0, 0, "BASIC",      "atom_basic.rom|BASIC.ROM|basic.rom") \
    V(ctx, ROMChip,    fp_rom, 0xD000,  2048,      0, 0, "FP ROM",     "?atom_fp.rom|FP.ROM|fp.rom")         \
    V(ctx, ROMChip,    os_rom, 0xF000,  4096,      0, 0, "OS ROM",     "atom_os.rom|ABASIC.ROM|os.rom")      \
    V(ctx, mc6847_t,   vdg,    0x0000,     0,      0, 0, "MC6847 VDG", nullptr)

static constexpr size_t kAtomChipCount = 0 ATOM_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr ChipManifest<kAtomChipCount> kAcornAtomChips = ChipManifest<kAtomChipCount>{{
    ATOM_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
}};

// BusSpec auto-derived from the manifest.  EnableCs=true enables CS-tick.
using AcornAtomBusSpec = ManifestBusSpec<kAcornAtomChips, 16, 8, 1, true>;

// ── Chips ──────────────────────────────────────────────────────────────
struct AtomChipset {
    ATOM_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
};

class AcornAtomSystem : public System {
public:
    AcornAtomSystem();
    ~AcornAtomSystem() override;

    // ── System interface ─────────────────────────────────────────
    const SystemDescriptor& get_descriptor() const override;
    bool set_configuration(const SystemConfiguration& config) override;
    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

private:
    // ── Chips (value-typed via Board Chips) ────────────────────────────

    // Direct pointer into flat mem for VDG rendering
    uint8_t* video_ram_ptr_ = nullptr;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using Bus = MemoryBus<AcornAtomBusSpec>;
    using PT  = PackingTraits<AcornAtomBusSpec>;
    using MainBoard = Board<AcornAtomBusSpec, AtomChipset>;
    Bus bus_;
    MainBoard board_{kAcornAtomChips};

    // ── Video ────────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // Video output

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[acorn_atom_constants::KEYBOARD_ROWS] = {};

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_      = ATOM_BUS_DEFAULT_STATE;
    uint32_t ram_size_kb_  = 2;          // configurable: 2, 5, 8, 12
    int audio_sample_rate_ = acorn_atom_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();  // Setup page tables for current config
    void render_frame();   // Render one complete video frame to framebuffer_
    bool load_roms();

    // PPI keyboard matrix callback — called before every PPI register read
    static uint8_t ppi_keyboard_scan(void* context, uint8_t port_a_output);
};
