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
#include <cstring>
#include <utility>
#include <vector>
#include <array>

#include "../../core/chip.h"
#include "../../core/system_lines.h"
#include "../../../systems/nes/bus/nes_bus.h"
#include "../../../systems/nes/bus/nes_bus_signals.h"
#include "nes_palette.h"

// Forward declarations
namespace nes_system {
    class Cartridge;
}

namespace nes_system {

class PPU : public ChipBase {
public:
    // PPU register indices (memory-mapped at $2000-$2007)
    static constexpr uint8_t PPUCTRL   = 0;  // $2000
    static constexpr uint8_t PPUMASK   = 1;  // $2001
    static constexpr uint8_t PPUSTATUS = 2;  // $2002
    static constexpr uint8_t OAMADDR   = 3;  // $2003
    static constexpr uint8_t OAMDATA   = 4;  // $2004
    static constexpr uint8_t PPUSCROLL = 5;  // $2005
    static constexpr uint8_t PPUADDR   = 6;  // $2006
    static constexpr uint8_t PPUDATA   = 7;  // $2007
    static constexpr uint8_t REG_COUNT = 8;

    uint8_t regs[REG_COUNT] = {};  // PPU register file

    // PPU memory
    //
    // CIRAM (2KB nametable VRAM) now lives in the unified buffer (nes_bus_t).
    // ciram_ is a non-owning pointer set by connect_bus().
    static constexpr size_t CIRAM_SIZE = nes_bus::CIRAM_SIZE;
    uint8_t* ciram_ = nullptr;

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

        // Sprite evaluation state machine — models per-cycle evaluation
        // during dots 65-256 on visible scanlines for accurate overflow
        // flag timing and the PPU's buggy overflow byte-offset behavior.
        struct SpriteEval {
            uint8_t n         = 0;  // Primary OAM sprite index (0-63)
            uint8_t m         = 0;  // Byte offset for overflow bug (0-3)
            uint8_t found     = 0;  // In-range sprites found (0-8)
            uint8_t copy_step = 0;  // 0=comparing, 1-3=copying remaining bytes
            uint8_t phase     = 0;  // 0=idle, 1=finding, 2=overflow check, 3=done
        } sprite_eval;
    } internal = {};

    // Timing
    int16_t scanline = -1;    // Current scanline (-1 to 260)
    uint16_t cycle = 0;       // Current cycle (0 to 340)
    uint64_t frame_count = 0; // Frame counter
    bool frame_complete = false;
    uint64_t last_vbl_detect_dot_ = 0; // Total dots when last $2002 VBL detect

    // PPU bus state flows through arguments.  bus_snapshot_ (inherited
    // from ChipBase) stores the PPU bus state between ticks — the system
    // reads it at tick entry and writes it back after clock() returns.
    // The PPU never stores a separate ppu_bus_ member; each function
    // receives the current state, modifies it, and returns it.

    // Open bus decay — on real hardware, the PPU's CPU-side data pins
    // have a capacitive latch (io_latch_) that retains whatever was last
    // driven during a PPU register access.  Between accesses, the latch
    // decays toward 0 due to parasitic capacitance discharging.
    //
    // We model only D0-D7 because those are the only decaying lines
    // observable through software — reads of PPU registers return stale
    // data bits for any bits the register doesn't actively drive.
    //
    // Per-bit decay period: ~600ms ≈ 3,221,590 PPU dots (NTSC).
    static constexpr uint64_t OPEN_BUS_DECAY_DOTS = 3'221'590;
    uint8_t io_latch_ = 0;                 // PPU internal data bus buffer
    uint64_t open_bus_refresh_[8] = {};     // Per-bit: PPU dot when last driven

    // VRAM data latch — bus-mediated rendering model.
    // Captures PPU bus data at the start of each clock() call.  Between
    // dots, the cartridge's ppu_memory_tick() reads the address the PPU
    // placed on the bus, performs block dispatch + A12 edge detection,
    // and places the result on the bus data lines.  The PPU captures
    // that data here and uses it on odd sub-cycles (1, 3, 5, 7).
    uint8_t vram_data_latch_ = 0;

    // VBL internal/external split for accurate PPU-CPU timing.
    //
    // On real hardware the VBL flip-flop is set at dot 1 of scanline 241.
    // NMI output asserts immediately (same PPU dot), but the flag doesn't
    // appear in $2002 reads until one PPU clock later (dot 2), due to an
    // internal propagation delay through the PPU status latch.
    //
    // We model this by keeping an internal state (`vbl_flag_internal_`)
    // that drives NMI and a pending flag (`pending_vbl_set_`) that commits
    // to regs[PPUSTATUS] bit 7 at the START of the next PPU::clock() call.
    // The same 1-dot delay applies to VBL clear at pre-render dot 1.
    bool     vbl_flag_internal_ = false;    // True = VBL active (drives NMI)
    bool     pending_vbl_set_ = false;      // Commit regs[PPUSTATUS] |= 0x80 next dot
    bool     pending_vbl_clear_ = false;    // Commit regs[PPUSTATUS] &= ~0x80 next dot

    // VBL suppression — reading $2002 within a 1-PPU-dot window BEFORE
    // VBL flag set (scanline 241, dot 1) prevents the flag from being set
    // that frame and suppresses NMI.  Reading AT the VBL dot returns 0
    // (flag not yet propagated) and suppresses the frame's VBL entirely.
    // Set true by service_cpu_bus on $2002 read; consumed at the start of
    // the next clock() call.  Replaces the old total_dots_/status_read_dot_
    // pair, avoiding 16 bytes of storage and integer overflow concerns.
    bool     status_read_last_dot_ = false;
    bool     vbl_was_suppressed_ = false;   // true if VBL never set this frame

    // Monotonic PPU dot counter — passed to the mapper on A12
    // transitions so it can implement its own timing filter.
    uint64_t ppu_dot_count_ = 0;

    // NOTE: Bus snapshots live on each chip's bus_snapshot_ (inherited
    // from ChipBase).  The PPU's bus_snapshot_ stores the PPU bus state
    // between dots; the system reads it at tick entry and writes it back
    // after clock() returns.  The PPU-side CPU data latch (io_latch_) is
    // internal to the PPU — the system doesn't need to manage it.

    // Region
    bool is_pal = false;
    int total_scanlines_minus_one_ = 261;  // NTSC: 262 - 1

    // Scanline event state machine — monotonically advances through
    // per-scanline point events (scroll, sprite eval, mapper, NT reads).
    // Replaces 6 individual cycle comparisons with a single switch dispatch.
    // Reset to 0 at each scanline wrap.
    uint8_t scanline_event_ = 0;

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

    // Precomputed pixel LUT — 32 entries, one per palette slot.
    // Collapses the per-pixel triple indirection (pal_mirror_ → palette →
    // active_palette) into a single array lookup.  Rebuilt lazily on first
    // render-path access after palette RAM writes or PPUMASK changes.
    uint32_t pixel_lut_[32] = {};

    // Active palette variant — pointer into palette_cache_.  Set to nullptr
    // to mark pixel_lut_ as stale; the render path checks this once per dot
    // with an unlikely branch and rebuilds on demand.  Coalesces rapid
    // palette/mask writes (common during setup) into a single rebuild.
    const uint32_t* active_palette_ = nullptr;

    // Frame buffer (RGB888)
    std::vector<uint32_t> screen;

public:
    PPU(bool pal = false) : is_pal(pal),
        total_scanlines_minus_one_(pal ? 311 : 261) {  // PAL: 312-1, NTSC: 262-1
        info_ = ChipInfo{pal ? "RP2C07" : "RP2C02", "Ricoh"};
        // Initialize PPU memory (std::array zero-initialized by {})
        screen.resize(256 * 240, 0);
        internal.sprite_scanline.fill({0xFF, 0xFF, 0xFF, 0xFF});
        internal.sprite_count = 0;

        build_palette_cache(is_pal, palette_cache_);
        reset();
        register_debug_fields();
    }

    void reset() {
        std::memset(regs, 0, sizeof(regs));
        internal = {};
        scanline = -1;
        cycle = 0;
        frame_count = 0;
        frame_complete = false;
        status_read_last_dot_ = false;
        vbl_was_suppressed_ = false;
        vbl_flag_internal_ = false;
        pending_vbl_set_ = false;
        pending_vbl_clear_ = false;
        scanline_event_ = 0;
        ppu_dot_count_ = 0;
        vram_data_latch_ = 0;
        std::fill(std::begin(open_bus_refresh_), std::end(open_bus_refresh_), 0);
        io_latch_ = 0;
        bus_snapshot_ = PPU_BUS_DEFAULT_STATE;   // PPU bus (active-low signals HIGH)

        // Clear memory
        if (ciram_) std::memset(ciram_, 0, CIRAM_SIZE);
        oam.fill(0);
        palette.fill(0);
        std::fill(screen.begin(), screen.end(), 0);

        build_palette_cache(is_pal, palette_cache_);
        rebuild_pixel_lut();
    }

    // Service a CPU bus cycle targeting the PPU register window ($2000-$2007).
    // Receives: cpu_bus (current CPU bus), ppu_bus (for NMI output updates).
    // Returns: {cpu_bus, ppu_bus} — caller stores both snapshots.
    std::pair<bus_state_t, ppu_bus_state_t> service_cpu_bus(
        bus_state_t cpu_bus, ppu_bus_state_t ppu_bus);

    // Read-only peek for debug/GUI (no side-effects on PPU state).
    uint8_t cpu_peek(uint16_t addr) const;

    // PPU memory access — direct addr/data interface.
    // Used by service_cpu_bus ($2007 handler) and setup utilities.
    uint8_t ppu_read_byte(uint16_t addr) const;
    void ppu_write_byte(uint16_t addr, uint8_t data);

    // Main PPU tick — one dot.  Receives the PPU bus state (snapshot
    // from end of previous tick), returns the updated PPU bus state.
    // The caller stores the returned value as the new PPU bus snapshot.
    ppu_bus_state_t clock(ppu_bus_state_t ppu_bus);

    // Connect cartridge for CHR data access and mapper interaction
    void connect_cartridge(Cartridge* cartridge);

    // Connect bus for page-pointer VRAM access and CIRAM pointer
    void connect_bus(nes_bus::nes_bus_t* bus) {
        bus_ptr_ = bus;
        if (bus) ciram_ = bus->ciram;
    }

    // Get frame buffer
    const std::vector<uint32_t>& get_screen() const { return screen; }

    // NMI output level — true when /NMI is asserted (active LOW).
    // Reads from the caller-provided ppu_bus; the system passes the
    // current PPU bus state after clock() returns.
    static bool nmi_output(ppu_bus_state_t ppu_bus) {
        return !PPU_BUS_GET_BIT(ppu_bus, BUS_NMI_BIT);
    }

private:
    Cartridge* cart_ = nullptr;
    nes_bus::nes_bus_t* bus_ptr_ = nullptr;   // Page-pointer bus for VRAM reads

    // CPU data bus helpers — model the capacitive retention on the
    // PPU's CPU-side data pins (io_latch_).  On real silicon every
    // driven transaction (read or write) recharges the bits that are
    // actively driven.  Address and control lines also decay but are
    // not observable through software, so we omit them.
    //
    // apply_open_bus_decay(bus) loads io_latch_ with per-bit decay onto
    // the bus's D0-D7, preserving address/R/W/control bits.
    inline bus_state_t apply_open_bus_decay(bus_state_t bus) const {
        uint8_t data = io_latch_;
        for (int i = 0; i < 8; i++) {
            if ((ppu_dot_count_ - open_bus_refresh_[i]) >= OPEN_BUS_DECAY_DOTS)
                data &= ~(1 << i);
        }
        BUS_SET_DATA(bus, data);
        return bus;
    }

    // Record that specific bits were actively driven this tick.
    // Updates timestamps AND the io_latch_ from the current bus data.
    inline void refresh_open_bus_timestamps(bus_state_t bus, uint8_t mask = 0xFF) {
        const uint8_t data = BUS_GET_DATA(bus);
        // Update latch: driven bits take new value, undriven bits retain
        io_latch_ = (io_latch_ & ~mask) | (data & mask);
        for (int i = 0; i < 8; i++) {
            if (mask & (1 << i))
                open_bus_refresh_[i] = ppu_dot_count_;
        }
    }

    // Const accessor — returns decayed io_latch_ value.  Used by cpu_peek.
    inline uint8_t decayed_latch_data() const {
        uint8_t data = io_latch_;
        for (int i = 0; i < 8; i++) {
            if ((ppu_dot_count_ - open_bus_refresh_[i]) >= OPEN_BUS_DECAY_DOTS)
                data &= ~(1 << i);
        }
        return data;
    }

    // Internal rendering functions
    void increment_scroll_x();
    void increment_scroll_y();
    void transfer_address_x();
    void transfer_address_y();
    void load_background_shifters();
    void update_shifters();

    // Invalidate pixel LUT — called on $2001 writes, palette RAM writes,
    // and reset.  The actual rebuild is deferred to the render path.
    // Lazily rebuild pixel_lut_ from current palette RAM and PPUMASK.
    // Only called when active_palette_ is null (i.e. after invalidation).
    void rebuild_pixel_lut() {
        const uint8_t variant = ((regs[PPUMASK] >> 5) & 0x07) | ((regs[PPUMASK] & 0x01) << 3);
        active_palette_ = palette_cache_[variant];
        for (int i = 0; i < 32; ++i)
            pixel_lut_[i] = active_palette_[palette[pal_mirror_[i]] & 0x3F];
    }

    // Sprite evaluation — monolithic (finds first 8 in-range sprites for
    // secondary OAM at cycle 257) + per-cycle state machine (sets overflow
    // flag at the correct dot during cycles 65-256 with buggy behavior).
    void evaluate_sprites();
    void sprite_eval_step();

    // Sprite pattern address calculation — returns the low-byte pattern
    // table address for the given sprite slot.  High byte is addr + 8.
    // Works for all 8 slots: unused slots ($FF OAM) produce valid PPU
    // bus addresses for correct A12 transitions.
    inline uint16_t compute_sprite_pattern_addr(uint8_t i) const {
        const auto& spr = internal.sprite_scanline[i];
        if (regs[PPUCTRL] & 0x20) {
            // 8x16 sprites
            int row = (scanline - spr.y) & 0x0F;
            if (spr.attributes & 0x80) row = 15 - row;  // vertical flip
            uint8_t tile_base = spr.tile_id & 0xFE;
            if (row >= 8) { tile_base++; row -= 8; }
            return ((spr.tile_id & 0x01) << 12) | (tile_base << 4) | row;
        } else {
            // 8x8 sprites
            int row = (scanline - spr.y) & 0x07;
            if (spr.attributes & 0x80) row = 7 - row;   // vertical flip
            return ((regs[PPUCTRL] & 0x08) << 9) | (spr.tile_id << 4) | row;
        }
    }

    // Horizontal bit-reverse for sprite rendering.
    static inline uint8_t flip_byte(uint8_t b) {
        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
        return b;
    }

    // Update /NMI output level on ppu_bus based on current VBL state
    // and NMI enable.  Called after any state change that affects NMI:
    //   - clock() VBL set/clear
    //   - $2002 read (clears VBL)
    //   - $2000 write (changes NMI enable)
    //
    // Uses vbl_flag_internal_ (set at dot 1) rather than regs[PPUSTATUS] bit 7
    // (visible at dot 2) so NMI asserts at the correct PPU clock.
    inline void update_nmi_output(ppu_bus_state_t& ppu_bus) {
        if (vbl_flag_internal_ && (regs[PPUCTRL] & 0x80)) {
            PPU_BUS_CLR_BIT(ppu_bus, BUS_NMI_BIT);  // active low = asserted
        } else {
            PPU_BUS_SET_BIT(ppu_bus, BUS_NMI_BIT);  // inactive high
        }
    }

    // --- ChipBase interface ---
public:
    bool has_settings_content() const override;
    bool has_layout_content()   const override;
    void render_settings_content() override;
    void render_layout_content()   override;

private:
    void register_debug_fields();
};

} // namespace nes_system
