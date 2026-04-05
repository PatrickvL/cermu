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
#include "core/signal/video_port.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"
#include "chip/video/mc6847/mc6847.hpp"
#include "chip/io/i8255.hpp"
#include "chip/io/mos6522.hpp"
#include "chip/memory/memory_chip.hpp"
#include <cstdint>

// Default bus state for the Atom — inherited from MOS 6502 defaults.
#define ATOM_BUS_DEFAULT_STATE (MOS6502::default_bus_state())

inline constexpr auto kAtomManifest = make_manifest(
    // Chips
    Slot<MOS6502>{.base_addr = 0x0000, .label = "MOS 6502"},
    Slot<RAMChip>{.base_addr = 0x0000, .size_bytes = 0x8000, .label = "RAM"},
    Slot<RAMChip>{.base_addr = 0x8000, .size_bytes = 0x2000, .label = "Video RAM"},
    Slot<i8255_t>{.base_addr = 0xB000, .addr_mask = 0xFFFC, .label = "i8255 PPI"},
    Slot<mos6522_t>{.base_addr = 0xB800, .addr_mask = 0xFFF0, .label = "VIA 6522"},
    Slot<ROMChip>{.base_addr = 0xC000, .size_bytes = 0x1000, .label = "BASIC", .rom = {"atom_basic.rom|BASIC.ROM|basic.rom"}},
    Slot<ROMChip>{.base_addr = 0xD000, .size_bytes = 0x0800, .label = "FP ROM", .rom = {"atom_fp.rom|FP.ROM|fp.rom", true}},
    Slot<ROMChip>{.base_addr = 0xF000, .size_bytes = 0x1000, .label = "OS ROM", .rom = {"atom_os.rom|ABASIC.ROM|os.rom"}},
    Slot<mc6847_t>{.base_addr = 0x0000, .label = "MC6847 VDG"},
    // Ports
    Slot<PortCassette>{.name = "Cassette Port"},
    Slot<PortExpansion>{.name = "Expansion Port"},
    Slot<PortCompositeVideo>{.name = "Video Out", .default_device = "crt_tv_pal"}
);

inline constexpr size_t kAtomChipCount = decltype(kAtomManifest)::chip_count;
// BusSpec auto-derived from the manifest.  EnableCs=true enables CS-tick.
using AcornAtomBusSpec = ManifestBusSpec<kAtomManifest, 16, 8, 1, true>;

struct AtomBoard : Board<AcornAtomBusSpec> {
    using ComponentTuple = decltype(kAtomManifest)::component_tuple;
    ComponentTuple components_;

    // Chip aliases
    MOS6502&   cpu    = std::get<0>(components_);
    RAMChip&   ram    = std::get<1>(components_);
    RAMChip&   vram   = std::get<2>(components_);
    i8255_t&   ppi    = std::get<3>(components_);
    mos6522_t& via    = std::get<4>(components_);
    ROMChip&   basic  = std::get<5>(components_);
    ROMChip&   fp_rom = std::get<6>(components_);
    ROMChip&   os_rom = std::get<7>(components_);
    mc6847_t&  vdg    = std::get<8>(components_);

    // Port aliases
    PortCassette&       cassette_port  = std::get<9>(components_);
    PortExpansion&      expansion_port = std::get<10>(components_);
    PortCompositeVideo& video_port     = std::get<11>(components_);

    AtomBoard() : Board(kAtomManifest) {}
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
    using MainBoard = AtomBoard;
    Bus bus_;
    MainBoard board_;

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
