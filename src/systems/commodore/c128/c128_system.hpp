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
#include "core/board.hpp"
#include "chip/cpu/fam65xx/csg8502.hpp"
#include "chip/cpu/z80/zilog_z80a.hpp"
#include "chip/video/vic_ii/vicii_common.hpp"
#include "chip/sound/mos6581.hpp"
#include "chip/io/mos6526.hpp"
#include "chip/memory/memory_chip.hpp"

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
//
// I/O at $D000-$DFFF handled by io_tick() when MMU selects I/O mode.
// VDC (8563) at $D600-$D601 — indirect register access (address/data latch).
// 8722 MMU at $D500-$D50B and $FF00-$FF04.
//
// Non-bus chips:
//   CSG 8502, Z80A, VIC-IIe, SID, CIA ×2
//
inline constexpr auto kC128Chips = make_chip_manifest(
    Slot<RAMChip>{0x0000, 131072, 0, "Main RAM", 0, 65536},  // 2 × 64 KB banks
    Slot<ROMChip>{0x4000,  16384, 0, "BASIC lo"}.with_rom("c128_basic_lo.rom|basic_lo.rom|basiclo.rom"),
    Slot<ROMChip>{0x8000,  16384, 0, "BASIC hi"}.with_rom("c128_basic_hi.rom|basic_hi.rom|basichi.rom"),
    Slot<ROMChip>{0xC000,   4096, 0, "Editor ROM"}.with_rom("c128_editor.rom|editor.rom"),
    Slot<ROMChip>{0xE000,   8192, 0, "Kernal ROM"}.with_rom("c128_kernal.rom|kernal.rom"),
    Slot<ROMChip>{0xD000,   4096, 0, "Character ROM"}.with_rom("c128_chargen.rom|chargen.rom|characters.rom"),
    // VDC video RAM — separate bus, not CPU-addressed
    Slot<RAMChip>{0x0000,  16384, 0, "VDC VRAM"},
    // Non-bus chips — factory-created, not address-decoded
    Slot<CSG8502>   {0, 0, 0, "CSG 8502"},
    Slot<ZilogZ80A> {0, 0, 0, "Zilog Z80A"},
    Slot<vicii_t>   {0, 0, 0, "MOS 8564 VIC-IIe"},
    Slot<mos6581_t> {0, 0, 0, "MOS 6581 SID"},
    Slot<mos6526_t> {0, 0, 0, "CIA 1"},
    Slot<mos6526_t> {0, 0, 0, "CIA 2"}
);

using C128BusSpec = ManifestBusSpec<kC128Chips, 16, 8>;

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

    void get_display_dimensions(int* width, int* height) const override;

    void handle_keyboard_event(SDL_Keycode key, bool pressed) override;

    uint32_t get_audio_samples(float* buffer, uint32_t max_samples) override;
    void set_audio_sample_rate(int sample_rate_hz) override;

    void render_system_menu_items() override;
    void render_configuration_ui() override;

    void set_speed_multiplier(float multiplier) override;

    bool is_system_ready() const override { return system_ready_; }

protected:
    // ── CommodoreSystem hooks ────────────────────────────────────
    bool is_basic_ready() const override;
    commodore_load_context_t build_load_context() override;
    void inject_keys(const char* str) override;
    bool is_system_initialized() const override { return system_ready_; }

private:
    // ── Chips ────────────────────────────────────────────────────────────
    CSG8502*    cpu_8502_ = nullptr;     // Primary CPU — 6502-compatible
    ZilogZ80A*  cpu_z80_  = nullptr;     // Secondary CPU — Z80 for CP/M
    vicii_t*    vic_      = nullptr;     // VIC-IIe (40-column display)
    mos6581_t*  sid_      = nullptr;     // SID sound chip
    mos6526_t*  cia1_     = nullptr;     // CIA 1 (keyboard + joystick)
    mos6526_t*  cia2_     = nullptr;     // CIA 2 (IEC serial + user port)

    // ── Memory chips — post-init pointers ────────────────────────────────
    ROMChip* basic_lo_rom_ = nullptr;
    ROMChip* basic_hi_rom_ = nullptr;
    ROMChip* editor_rom_   = nullptr;
    ROMChip* kernal_rom_   = nullptr;
    ROMChip* char_rom_     = nullptr;
    RAMChip* vdc_vram_     = nullptr;    // 16KB VDC video RAM

    // ── Board + bus ──────────────────────────────────────────────────────
    using Bus       = MemoryBus<C128BusSpec>;
    using MainBoard = Board<C128BusSpec>;
    Bus       bus_;
    MainBoard board_{kC128Chips};

    // ── Display ──────────────────────────────────────────────────────────
    IndexedFrameBuffer display_;          // VIC-IIe output (40-col)
    // TODO: second display for VDC 80-column output

    // ── 8722 MMU state ───────────────────────────────────────────────────
    uint8_t mmu_cr_        = 0;          // Configuration register
    uint8_t mmu_pcr_[4]    = {};         // Preconfiguration registers A-D
    uint8_t mmu_mcr_       = 0;          // Mode config register
    uint8_t mmu_rcr_       = 0;          // RAM config register
    uint8_t mmu_p0_[2]     = {};         // Page 0 pointer (lo/hi)
    uint8_t mmu_p1_[2]     = {0, 1};    // Page 1 pointer (lo/hi) — default $0100

    // ── CPU mode ─────────────────────────────────────────────────────────
    enum class CPUMode : uint8_t { MODE_8502, MODE_Z80 };
    CPUMode  cpu_mode_ = CPUMode::MODE_8502;
    bool     c64_mode_ = false;          // C64 compatibility mode

    // ── System state ─────────────────────────────────────────────────────
    bus_state_t pins_      = C128_BUS_DEFAULT_STATE;
    bool        system_ready_ = false;
    int         audio_sample_rate_ = c128_constants::DEFAULT_SAMPLE_RATE;

    // ── Internal helpers ─────────────────────────────────────────────────
    void configure_bus_memory_map();
    bool load_roms();
    void mmu_write(uint16_t addr, uint8_t data);
    uint8_t mmu_read(uint16_t addr);
    void update_bank_config();           // Apply MMU state to page tables
    void switch_cpu_mode(CPUMode mode);  // Toggle between 8502 and Z80
};
