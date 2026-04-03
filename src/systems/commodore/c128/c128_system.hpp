#pragma once
/*
 * c128_system.h — Commodore 128 system declaration
 *
 * The Commodore 128 (1985) is a dual-CPU, multi-mode home computer:
 *   - C128 mode: CSG 8502 @ 1/2 MHz, BASIC 7.0, 128KB RAM
 *   - C64 mode:  Full hardware C64 compatibility
 *   - CP/M mode: Z80 @ 4 MHz running CP/M 3.0
 *
 * Hardware:
 *   CPU:    CSG 8502 (1/2 MHz) + Zilog Z80 (4 MHz) — only one active at a time
 *   Video:  MOS 8564/8566 VIC-IIe (40-col, C64-compatible) +
 *           MOS 8563 VDC (80-col, 16KB dedicated VRAM) — active simultaneously
 *   Sound:  MOS 6581 or 8580 SID
 *   I/O:    2× MOS 6526 CIA (keyboard, joystick, IEC serial, user port)
 *   MMU:    MOS 8722 (bank switching, mode control, zero/stack page relocation)
 *   RAM:    128KB main + 16KB VDC VRAM (separate bus)
 *   ROM:    64KB (BASIC 7.0 lo+hi, KERNAL, character ROM, editor, CP/M boot)
 *
 * Memory map (C128 mode, default config):
 *   $0000-$3FFF : RAM (bank 0 or 1, selected by 8722 MMU)
 *   $4000-$7FFF : BASIC 7.0 low ROM or RAM
 *   $8000-$BFFF : BASIC 7.0 high ROM or RAM
 *   $C000-$CFFF : Editor ROM or RAM
 *   $D000-$DFFF : I/O or Character ROM or RAM (8722 MCR controlled)
 *   $E000-$FFFF : KERNAL ROM or RAM
 *
 * The 8722 MMU provides full 128KB RAM banking with independent control
 * over each 16KB region.  The VIC-IIe always sees 64KB (bank 0 or 1).
 * The Z80 sees the same address space through a transparent bus bridge.
 * The VDC has its own private 16KB VRAM, accessed via register latch.
 */

#include "systems/commodore/c128/c128_constants.hpp"
#include "systems/commodore/commodore_system.hpp"
#include "systems/commodore/c128/c128_keyboard_matrix.hpp"
#include "core/board.hpp"
#include "core/system_chip_visitors.hpp"
#include "core/signal/video_port.hpp"
#include "chip/cpu/fam65xx/csg8502.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/vic_ii/mos8566.hpp"
#include "chip/video/fam6845/mos8563.hpp"
#include "chip/sound/mos6581.hpp"
#include "chip/io/mos6526.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/memory/mos2114.hpp"
#include "chip/mmu/mos8722.hpp"

#include <atomic>
#include <cstdint>

// Default bus state — CSG 8502 (same pinout as MOS 6510)
#define C128_BUS_DEFAULT_STATE (CSG8502::default_bus_state())

// =============================================================================
// C128 chip manifest — declarative memory layout
// =============================================================================
//
// Memory-mapped chips:
//   Slot 0: Main RAM    — 128 KB at $0000 (2 × 64 KB banks)
//   Slot 1: BASIC lo    —  16 KB at $4000 (BASIC 7.0 low half)
//   Slot 2: BASIC hi    —  16 KB at $8000 (BASIC 7.0 high half)
//   Slot 3: Editor ROM  —   4 KB at $C000
//   Slot 4: Kernal ROM  —   8 KB at $E000
//   Slot 5: Char ROM    —   4 KB at $D000 (banked via MMU, not always visible)
//   Slot 6: Z80 BIOS    —   4 KB at $0000 (Z80 only, from KERNAL chip offset $1000)
//
// I/O at $D000-$DFFF handled by io_tick() when MMU selects I/O mode.
// VDC (8563) at $D600-$D601 — indirect register access (address/data latch).
// 8722 MMU at $D500-$D50B and $FF00-$FF04.
//
// Non-bus chips:
//   CSG 8502, Z80A
//
//                                ctx   type       chip        base    size    mask  ovl  label              rom
#define C128_FOR_EACH_SYSTEM_CHIP(V, ctx) \
    V(ctx, CSG8502,    csg8502,     0,            0, 0,      0, "CSG 8502",        nullptr) \
    V(ctx, RAMChip,    main_ram,    0x0000, 131072, 0,      0, "Main RAM",        nullptr) \
    V(ctx, ROMChip,    basic_lo,    0x4000,  16384, 0,      1, "BASIC lo",        "basic-4000.318018-04.bin|c128_basic_lo.rom|basic_lo.rom|basiclo.rom|basic.318023-02.bin@0") \
    V(ctx, ROMChip,    basic_hi,    0x8000,  16384, 0,      1, "BASIC hi",        "basic-8000.318019-04.bin|c128_basic_hi.rom|basic_hi.rom|basichi.rom|basic.318023-02.bin@16384") \
    V(ctx, ROMChip,    editor_rom,  0xC000,   4096, 0,      1, "Editor ROM",      "c128_editor.rom|editor.rom|kernal.318020-05.bin@0") \
    V(ctx, ROMChip,    char_rom,    0xD000,   8192, 0,      1, "Character ROM",   "characters.390059-01.bin|c128_chargen.rom|chargen.rom|characters.rom") \
    V(ctx, ROMChip,    kernal_rom,  0xE000,   8192, 0,      1, "Kernal ROM",      "c128_kernal.rom|kernal.rom|kernal.318020-05.bin@8192") \
    V(ctx, ROMChip,    z80_bios,    0x0000,   4096, 0,      1, "Z80 BIOS",        "z80bios-128.rom|z80bios.rom|kernal.318020-05.bin@4096") \
    V(ctx, ROMChip,    c64_basic,   0xA000,   8192, 0,      1, "C64 BASIC",       "basic64-901226-01.bin|basic.901226-01.bin|c64_basic.rom") \
    V(ctx, ROMChip,    c64_kernal,  0xE000,   8192, 0,      1, "C64 Kernal",      "kernal64-901227-03.bin|kernal.901227-03.bin|c64_kernal.rom") \
    V(ctx, RAMChip,    vdc_vram,    0x0000,  16384, 0,      1, "VDC VRAM",        nullptr) \
    V(ctx, ZilogZ80A,  z80,         0,            0, 0,      0, "Zilog Z80A",      nullptr) \
    V(ctx, mos8566_t,  vic_iie,     0xD000,       0, 0,      0, "MOS 8566 VIC-IIe", nullptr) \
    V(ctx, mos6581_t,  sid,         0xD400,       0, 0,      0, "MOS 6581 SID",    nullptr) \
    V(ctx, MOS2114,    colorram,    0xD800,       0, 0,      0, "Color RAM",       nullptr) \
    V(ctx, mos8563_t,  vdc,         0xD600,       0, 0,      0, "MOS 8563 VDC",   nullptr) \
    V(ctx, mos8722_t,  mmu,         0xD500,       0, 0,      0, "MOS 8722 MMU",   nullptr) \
    V(ctx, mos6526_t,  cia1,        0xDC00,       0, 0,      0, "CIA 1",           nullptr) \
    V(ctx, mos6526_t,  cia2,        0xDD00,       0, 0,      0, "CIA 2",           nullptr)

static constexpr size_t kC128ChipCount = 0 C128_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_COUNT_ONE, unused);

inline constexpr auto make_c128_manifest() {
    ChipManifest<kC128ChipCount> m = {{
        C128_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_MANIFEST_ROW, unused)
    }};
    // Main RAM: 2 × 64 KB banks
    m.chips[1].bank_size = 65536;
    // All overlay-group 1 chips: bank_size = size_bytes (single bank each)
    for (auto& s : m.chips)
        if (s.overlay_group == 1) s.bank_size = s.size_bytes;
    // MMIO mirror ranges
    m.chips[12].bank_size = 0x400;   // VIC-IIe: mirrors across $D000-$D3FF
    m.chips[13].bank_size = 0x400;   // SID: mirrors across $D400-$D7FF
    return m;
}
inline constexpr auto kC128Chips = make_c128_manifest();

// 4 KB pages, 2 viewers (CPU + VIC-IIe), CS-tick enabled
struct C128BusSpec : ManifestBusSpec<kC128Chips, 16, 12, 2, true> {
    static constexpr size_t MaxIndexedSubTables = 1;    // I/O page ($D000-$DFFF)
    static constexpr size_t IndexedSubBits      = 4;    // 16 × 256B entries (bits 11-8)
};

// Value-typed chips: all chips are fields via X-macro.
struct C128Chipset {
    C128_FOR_EACH_SYSTEM_CHIP(CERMU_CHIP_VISITOR_DECLARE_FIELD, unused)
};

// =============================================================================
// C128 System
// =============================================================================

class C128System : public CommodoreSystem {
public:
    C128System();
    ~C128System() override;

    // ── System interface ─────────────────────────────────────────
    const SystemDescriptor& get_descriptor() const override;

    bool apply_configuration() override;

    bool initialize() override;
    void shutdown() override;
    void reset() override;

    void tick() override;
    void run_frame() override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void* get_video_port_ptr() override { return video_port_.get(); }

    void render_system_menu_items() override;
    const char* get_mode_label() const override;

protected:
    // ── CommodoreSystem hooks ────────────────────────────────────
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    void inject_keys(const char* str) override;
    bool is_system_initialized() const override { return system_ready_; }

private:
    // ── Memory chips — post-init pointers ────────────────────────────────
    ROMChip* basic_lo_rom_ = nullptr;
    ROMChip* basic_hi_rom_ = nullptr;
    ROMChip* editor_rom_   = nullptr;
    ROMChip* kernal_rom_   = nullptr;
    ROMChip* char_rom_     = nullptr;
    ROMChip* c64_basic_rom_  = nullptr;  // C64 BASIC V2 (8KB, for C64 mode)
    ROMChip* c64_kernal_rom_ = nullptr;  // C64 KERNAL (8KB, for C64 mode)
    ROMChip* z80_bios_rom_   = nullptr;  // Z80 BIOS (4KB, from KERNAL chip)
    RAMChip* vdc_vram_     = nullptr;    // 16KB VDC video RAM

    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<C128BusSpec>;
    using MainBoard = Board<C128BusSpec, C128Chipset>;
    Bus       bus_;
    MainBoard board_{kC128Chips};

    // ── Display ──────────────────────────────────────────────────────────
    std::unique_ptr<CompositeVideoPort> video_port_;  // VIC-IIe output (40-col)
    // VDC RGBI port: deferred until dual-display pipeline is implemented.
    // The VDC ticks counters and services MMIO without pixel output.

    // Cached processor port bits for bank config interaction
    uint8_t cpu_port_bits_  = 0x07;      // LORAM|HIRAM|CHAREN defaults (all high)

    // ── CPU mode ─────────────────────────────────────────────────────────
    enum class CPUMode : uint8_t { MODE_8502, MODE_Z80 };
    CPUMode       cpu_mode_ = CPUMode::MODE_Z80;  // Z80 starts first after reset
    CpuChipBase*  active_cpu_ = nullptr;           // Points to whichever CPU is active
    bool          c64_mode_ = false;               // C64 compatibility mode

    // ── GUI-requested actions (set on GUI thread, consumed on emu thread) ─
    std::atomic<bool> reset_requested_{false};
    std::atomic<bool> c64_mode_requested_{false};
    // ── System state ─────────────────────────────────────────────────────
    bus_state_t default_state_ = 0;     // Pull-up defaults (reset each tick)
    bus_state_t pins_      = C128_BUS_DEFAULT_STATE;  // 8502 bus state
    bus_state_t z80_pins_  = 0;                       // Z80 bus state
    int         audio_sample_rate_ = c128_constants::DEFAULT_SAMPLE_RATE;

    // ── Viewer IDs ───────────────────────────────────────────────────────
    static constexpr size_t kViewerCpu   = 0;
    static constexpr size_t kViewerVicII = 1;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bool load_roms();
    void update_bank_config();           // Apply MMU CR + RCR to page tables
    void enter_c64_mode();               // Transition to C64 compatibility mode
    void switch_cpu_mode(CPUMode mode);  // Toggle between 8502 and Z80
    void init_io_dispatch();             // Set up CS-tick indexed sub-table for I/O page
    void tick_z80();                     // Z80 tick (T-state) + bus servicing
    bus_state_t z80_io_tick(bus_state_t pins); // Z80 I/O port dispatch
    static void cpu_banking_callback(void* ctx, uint8_t banking_state);

    // CIA1 keyboard matrix scan callbacks
    static uint8_t c128_cia1_port_a_read(void* context, uint8_t port_a_output);
    static uint8_t c128_cia1_port_b_read(void* context, uint8_t port_b_output);
};
