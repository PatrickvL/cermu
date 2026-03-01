#pragma once
/*
 * nes_ppu.h — NES PPU (RP2C02 / RP2C07) : ChipBase
 *
 * Picture Processing Unit — generates the NES video output.
 *
 * This header declares the PPU class extracted from the monolithic
 * nes_system.h.  The implementation remains in nes_system.cpp during
 * Phase 1 of the migration; it will move to ppu/nes_ppu.cpp in step 1.5.
 *
 * The PPU is a ChipBase subclass with:
 *   - CPU bus interface (register reads/writes at $2000-$2007)
 *   - Internal PPU memory bus (VRAM, palette, CHR via cartridge)
 *   - Cycle-accurate scanline rendering (background + sprites)
 *   - Debug/settings/layout rendering through ChipBase GUI virtuals
 */

#include <cstdint>
#include <memory>
#include <vector>
#include <array>

#include "../../core/chip.h"
#include "../../core/system_lines.h"
#include "../bus/nes_bus.h"
#include "../bus/nes_bus_signals.h"
#include "nes_palette.h"

// Forward declarations
namespace nes_system {
    class Cartridge;
}

namespace nes_system {

class PPU : public ChipBase {
public:
    // Screen dimensions
    // PPU registers (memory-mapped at $2000-$2007)
    struct Registers {
        uint8_t ctrl;       // $2000 - PPUCTRL
        uint8_t mask;       // $2001 - PPUMASK
        uint8_t status;     // $2002 - PPUSTATUS
        uint8_t oam_addr;   // $2003 - OAMADDR
        uint8_t oam_data;   // $2004 - OAMDATA
        uint8_t scroll;     // $2005 - PPUSCROLL
        uint8_t addr;       // $2006 - PPUADDR
        uint8_t data;       // $2007 - PPUDATA
    } regs = {};

    // PPU memory — fixed-size arrays (sizes are hardware constants)
    alignas(64) std::array<uint8_t, 2048> vram{};    // 2KB VRAM (CIRAM — nametable RAM)
    std::array<uint8_t, 256> oam{};                   // 256 bytes OAM (Object Attribute Memory)
    std::array<uint8_t, 32> palette{};                // 32 bytes palette RAM

    // Internal state
    struct InternalState {
        uint16_t v = 0;       // Current VRAM address (15 bits)
        uint16_t t = 0;       // Temporary VRAM address (15 bits)
        uint8_t x = 0;        // Fine X scroll (3 bits)
        bool w = false;       // First or second write toggle
        uint8_t fine_y = 0;   // Fine Y scroll

        // Background rendering
        uint16_t nt_addr = 0;     // Nametable address
        uint8_t nt_byte = 0;      // Nametable byte
        uint8_t at_byte = 0;      // Attribute table byte
        uint8_t bg_lo_byte = 0;   // Background pattern table low
        uint8_t bg_hi_byte = 0;   // Background pattern table high

        // Background shift registers
        uint16_t bg_shifter_pattern_lo = 0;
        uint16_t bg_shifter_pattern_hi = 0;
        uint16_t bg_shifter_attrib_lo = 0;
        uint16_t bg_shifter_attrib_hi = 0;

        // Sprite rendering
        struct Sprite {
            uint8_t y = 0;
            uint8_t tile_id = 0;
            uint8_t attributes = 0;
            uint8_t x = 0;
        };

        std::array<Sprite, 8> sprite_scanline;  // Sprites for current scanline
        uint8_t sprite_count = 0;                  // Sprites found during evaluation
        uint8_t sprite_shifter_pattern_lo[8] = {};
        uint8_t sprite_shifter_pattern_hi[8] = {};

        bool sprite_zero_hit_possible = false;
        bool sprite_zero_being_rendered = false;
    } internal = {};

    // Timing
    int16_t scanline = -1;    // Current scanline (-1 to 260)
    uint16_t cycle = 0;       // Current cycle (0 to 340)
    uint64_t frame_count = 0; // Frame counter
    bool frame_complete = false;
    uint64_t last_vbl_detect_dot_ = 0; // Total dots when last $2002 VBL detect

    // PPU bus word — carries the PPU's output signal levels.
    // The NMI bit is driven as a continuous level here; the CPU's
    // internal edge-detect flip-flop handles the HIGH→LOW transition.
    // Shared bits (/NMI, /IRQ, /RES) are at the same positions as the
    // CPU bus_state_t, enabling zero-cost PPU_CPU_BITMIX transfer.
    ppu_bus_state_t ppu_bus_ = PPU_BUS_DEFAULT_STATE;

    // VBL internal/external split for accurate PPU-CPU timing.
    //
    // On real hardware the VBL flip-flop is set at dot 1 of scanline 241.
    // NMI output asserts immediately (same PPU dot), but the flag doesn't
    // appear in $2002 reads until one PPU clock later (dot 2), due to an
    // internal propagation delay through the PPU status latch.
    //
    // We model this by keeping an internal state (`vbl_flag_internal_`)
    // that drives NMI and a pending flag (`pending_vbl_set_`) that commits
    // to `regs.status` bit 7 at the START of the next PPU::clock() call.
    // The same 1-dot delay applies to VBL clear at pre-render dot 1.
    bool     vbl_flag_internal_ = false;    // True = VBL active (drives NMI)
    bool     pending_vbl_set_ = false;      // Commit regs.status |= 0x80 next dot
    bool     pending_vbl_clear_ = false;    // Commit regs.status &= ~0x80 next dot

    // VBL suppression — reading $2002 within a 1-PPU-dot window BEFORE
    // VBL flag set (scanline 241, dot 1) prevents the flag from being set
    // that frame and suppresses NMI.  Reading AT the VBL dot returns 0
    // (flag not yet propagated) and suppresses the frame's VBL entirely.
    // Set true by cpu_bus_tick on $2002 read; consumed at the start of
    // the next clock() call.  Replaces the old total_dots_/status_read_dot_
    // pair, avoiding 16 bytes of storage and integer overflow concerns.
    bool     status_read_last_dot_ = false;
    bool     vbl_was_suppressed_ = false;   // true if VBL never set this frame

    // Open bus data latch is stored in the data bits (0-7) of ppu_bus_.
    // These bits are otherwise unused — the rendering pipeline operates
    // on local ppu_bus_state_t values, and only the shared NMI/IRQ/RES
    // bits (32-34) are read externally via PPU_CPU_BITMIX.

    // Region
    bool is_pal = false;

    // Palette mirror LUT — folds $3F10/$3F14/$3F18/$3F1C → $3F00/04/08/0C.
    // Shared by ppu_read(), ppu_write(), and the per-pixel compositor.
    static constexpr uint8_t pal_mirror_[32] = {
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
         0,17,18,19, 4,21,22,23, 8,25,26,27,12,29,30,31
    };

    // Precalculated palette cache — 16 variants × 64 ABGR entries.
    // Index: variant = ((PPUMASK >> 5) & 0x07) | ((PPUMASK & 0x01) << 3)
    // Rebuilt on construction, reset, and PAL/NTSC change.
    uint32_t palette_cache_[16][64] = {};

    // Active palette variant — pointer into palette_cache_, latched on
    // $2001 (PPUMASK) writes so rebuild_pixel_lut() picks the right row.
    const uint32_t* active_palette_ = palette_cache_[0];

    // Precomputed pixel LUT — 32 entries, one per palette slot.
    // Collapses the per-pixel triple indirection (pal_mirror_ → palette →
    // active_palette_) into a single array lookup.  Rebuilt on palette RAM
    // writes ($2007 / DMA into $3F00+) and PPUMASK ($2001) writes.
    uint32_t pixel_lut_[32] = {};

    // Frame buffer (RGB888)
    std::vector<uint32_t> screen;

public:
    PPU(bool pal = false) : is_pal(pal) {
        info_ = ChipInfo{pal ? "RP2C07" : "RP2C02", "Ricoh"};
        // Initialize PPU memory (std::array zero-initialized by {})
        screen.resize(256 * 240, 0);
        internal.sprite_scanline.fill({0xFF, 0xFF, 0xFF, 0xFF});
        internal.sprite_count = 0;

        build_palette_cache(is_pal, palette_cache_);
        reset();
    }

    void reset() {
        regs = {};
        internal = {};
        scanline = -1;
        cycle = 0;
        frame_count = 0;
        frame_complete = false;
        ppu_bus_ = PPU_BUS_DEFAULT_STATE;
        status_read_last_dot_ = false;
        vbl_was_suppressed_ = false;
        vbl_flag_internal_ = false;
        pending_vbl_set_ = false;
        pending_vbl_clear_ = false;

        // Clear memory
        vram.fill(0);
        oam.fill(0);
        palette.fill(0);
        std::fill(screen.begin(), screen.end(), 0);

        build_palette_cache(is_pal, palette_cache_);
        rebuild_pixel_lut();
    }

    // CPU bus interface — the PPU is a bus device; it samples A0-A2, R/W
    // and drives/samples D0-D7 via the shared bus_state_t.  Open-bus behavior
    // emerges naturally because the data lines retain their last value.
    bus_state_t cpu_bus_tick(bus_state_t bus);

    // Read-only peek for debug/GUI (no side-effects on PPU state)
    uint8_t cpu_peek(uint16_t addr) const;

    // PPU memory access — bus_state_t receiving/returning pattern
    ppu_bus_state_t ppu_read(ppu_bus_state_t bus, bool read_only = false);
    ppu_bus_state_t ppu_write(ppu_bus_state_t bus);

    // Main PPU tick - called 3 times per CPU cycle
    void clock();

    // Connect cartridge for CHR data access and mapper interaction
    void connect_cartridge(std::shared_ptr<Cartridge> cartridge);

    // Connect bus for page-pointer VRAM access
    void connect_bus(nes_bus::nes_bus_t* bus) { bus_ptr_ = bus; }

    // Get frame buffer
    const std::vector<uint32_t>& get_screen() const { return screen; }

    // NMI output level — true when /NMI is asserted (active LOW).
    // The system tick transfers this onto the CPU bus via PPU_CPU_BITMIX;
    // the CPU's own edge-detect flip-flop handles the rest.
    bool nmi_output() const {
        return !PPU_BUS_GET_BIT(ppu_bus_, BUS_NMI_BIT);
    }

private:
    std::shared_ptr<Cartridge> cart;
    nes_bus::nes_bus_t* bus_ptr_ = nullptr;   // Page-pointer bus for VRAM reads

    // A12 edge detection — tracks last PPU address for mapper use
    uint16_t last_ppu_addr_ = 0;

    // Internal rendering functions
    void increment_scroll_x();
    void increment_scroll_y();
    void transfer_address_x();
    void transfer_address_y();
    void load_background_shifters();
    void update_shifters();

    // Rebuild the 32-entry pixel LUT from current palette RAM and PPUMASK.
    // Called on $2001 writes, palette RAM writes ($3F00+), and reset.
    // Subsumes the old latch_palette_variant() — both triggers converge here.
    inline void rebuild_pixel_lut() {
        const uint8_t variant = ((regs.mask >> 5) & 0x07) | ((regs.mask & 0x01) << 3);
        active_palette_ = palette_cache_[variant];
        for (int i = 0; i < 32; ++i)
            pixel_lut_[i] = active_palette_[palette[pal_mirror_[i]] & 0x3F];
    }

    // Sprite evaluation
    void evaluate_sprites();
    void load_sprite_shifters();

    // Update /NMI output level on ppu_bus_ based on current VBL state
    // and NMI enable.  Called after any state change that affects NMI:
    //   - clock() VBL set/clear
    //   - $2002 read (clears VBL)
    //   - $2000 write (changes NMI enable)
    //
    // Uses vbl_flag_internal_ (set at dot 1) rather than regs.status bit 7
    // (visible at dot 2) so NMI asserts at the correct PPU clock.
    inline void update_nmi_output() {
        if (vbl_flag_internal_ && (regs.ctrl & 0x80)) {
            PPU_BUS_CLR_BIT(ppu_bus_, BUS_NMI_BIT);  // active low = asserted
        } else {
            PPU_BUS_SET_BIT(ppu_bus_, BUS_NMI_BIT);  // inactive high
        }
    }

    // --- ChipBase interface ---
public:
    bool has_debug_content()    const override;
    bool has_settings_content() const override;
    bool has_layout_content()   const override;
    void render_debug_content()    override;
    void render_settings_content() override;
    void render_layout_content()   override;
};

} // namespace nes_system
