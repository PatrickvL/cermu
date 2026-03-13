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
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/video/mc6847/mc6847.hpp"
#include "chip/io/i8255.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>

// Default bus state for the Atom — inherited from MOS 6502 defaults.
#define ATOM_BUS_DEFAULT_STATE (MOS6502::default_bus_state())


// =============================================================================
// Acorn Atom chip manifest — declarative memory layout
// =============================================================================
//
// Slot 0: RAM       — 32 KB at $0000 (covers full lower half; actual size configurable)
// Slot 1: Video RAM — 8 KB at $8000 (real hardware: 6 KB at $8000–$97FF)
// Slot 2: BASIC ROM — 4 KB at $C000
// Slot 3: FP ROM    — 2 KB at $D000
// Slot 4: OS ROM    — 4 KB at $F000
// Slot 5: PPI       — MMIO-only, 4-byte window at $B000
// Slot 6: VIA       — MMIO-only, 16-byte window at $B800
//
// RAM is allocated as 32 KB (power of 2) covering $0000–$7FFF; the system
// trims actual read/write pages to the configured size (2/5/8/12 KB).
// Video RAM is 8 KB (power of 2); only $8000–$97FF is used by the MC6847.
//
inline constexpr auto kAcornAtomChips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 32768, 0, "RAM"},
    Slot<RAMChip>{0x8000,  8192, 0, "Video RAM"},
    Slot<ROMChip>{0xC000,  4096, 0, "BASIC"},
    Slot<ROMChip>{0xD000,  2048, 0, "FP ROM"},
    Slot<ROMChip>{0xF000,  4096, 0, "OS ROM"},
    Slot<i8255_t>   {0xB000,     0, 0xFFFC},           // MMIO-only, 4-byte window
    Slot<mos6522_t> {0xB800,     0, 0xFFF0},           // MMIO-only, 16-byte window
    // Non-bus chip — factory-created, not address-decoded
    Slot<MOS6502>   {0, 0, 0, "MOS 6502"}
);

// BusSpec auto-derived from the manifest
using AcornAtomBusSpec = ManifestBusSpec<kAcornAtomChips, 16, 8>;

namespace acorn_atom_chips {
    inline constexpr size_t kRamSlot      = 0;
    inline constexpr size_t kVideoRamSlot = 1;
    inline constexpr size_t kBasicSlot    = 2;
    inline constexpr size_t kFpRomSlot    = 3;
    inline constexpr size_t kOsRomSlot    = 4;
    inline constexpr size_t kPpiSlot      = 5;
    inline constexpr size_t kViaSlot      = 6;
    inline constexpr size_t kCpuSlot      = 7;

    // Compile-time chip ids (from manifest prefix-sum)
    inline constexpr size_t kRamId      = kAcornAtomChips.base_id(kRamSlot, AcornAtomBusSpec::PageBits);
    inline constexpr size_t kVideoRamId = kAcornAtomChips.base_id(kVideoRamSlot, AcornAtomBusSpec::PageBits);
    inline constexpr size_t kBasicId    = kAcornAtomChips.base_id(kBasicSlot, AcornAtomBusSpec::PageBits);
    inline constexpr size_t kFpRomId    = kAcornAtomChips.base_id(kFpRomSlot, AcornAtomBusSpec::PageBits);
    inline constexpr size_t kOsRomId    = kAcornAtomChips.base_id(kOsRomSlot, AcornAtomBusSpec::PageBits);
}

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

    bool load_file(const char* filepath) override;

    uint32_t* get_framebuffer() override;
    void get_display_dimensions(int* width, int* height) const override;
    void set_framebuffer(uint32_t* buffer, int width, int height) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;
    void render_system_menu_items() override;
    void render_configuration_ui() override;
    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    MOS6502*    cpu_  = nullptr;     // MOS 6502 — owned by bus_mem_
    mc6847_t    vdg_;                // MC6847 Video Display Generator — stack member
    i8255_t     ppi_;                // Intel 8255 PPI (keyboard + cassette ctrl)
    mos6522_t   via_;                // MOS 6522 VIA (timers, cassette, printer)

    // ── Memory chips — post-init pointers via chip_as<>() ───────────────
    ROMChip* basic_rom_ = nullptr;  // 4 KB at $C000
    ROMChip* fp_rom_    = nullptr;  // 2 KB at $D000
    ROMChip* os_rom_    = nullptr;  // 4 KB at $F000

    // Direct pointer into unified buffer for VDG rendering
    uint8_t* video_ram_ptr_ = nullptr;

    // ── MemoryBus — declarative setup via chip manifest ──────────────────
    using Bus = MemoryBus<AcornAtomBusSpec>;
    using PT  = PackingTraits<AcornAtomBusSpec>;
    using Mem = Board<AcornAtomBusSpec>;
    Bus bus_;
    Mem bus_mem_{kAcornAtomChips};

    // ── Display ──────────────────────────────────────────────────────────
    uint32_t framebuffer_[acorn_atom_constants::FB_WIDTH *
                          acorn_atom_constants::FB_HEIGHT] = {};

    // ── Keyboard ─────────────────────────────────────────────────────────
    uint8_t keyboard_matrix_[acorn_atom_constants::KEYBOARD_ROWS] = {};

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_      = ATOM_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    uint64_t total_cycles_ = 0;
    uint32_t ram_size_kb_  = 2;          // configurable: 2, 5, 8, 12
    int audio_sample_rate_ = acorn_atom_constants::DEFAULT_SAMPLE_RATE;
    float speed_multiplier_ = 1.0f;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();  // Setup page tables for current config
    void render_frame();   // Render one complete video frame to framebuffer_
    bool load_roms();

    // PPI keyboard matrix callback — called before every PPI register read
    static uint8_t ppi_keyboard_scan(void* context, uint8_t port_a_output);
};
