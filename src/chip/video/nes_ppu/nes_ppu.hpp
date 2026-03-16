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

#include "chip/video/video_chip_base.hpp"
#include "core/system_lines.hpp"
#include "core/indexed_frame_buffer.hpp"
#include "systems/nes/bus/nes_bus.hpp"
#include "systems/nes/bus/nes_bus_signals.hpp"
#include "chip/video/nes_ppu/nes_palette.hpp"

// Forward declarations
namespace nes_system {
    class Cartridge;
}

// ============================================================================
// NES PPU REGISTER TABLE — single source of truth
// ============================================================================

// DECL(REG, FLD, CMP) — 8 registers, 18 fields (PPUCTRL/PPUMASK/PPUSTATUS bits)
#define NES_PPU_DECL(REG, FLD, CMP) \
    REG(0, PPUCTRL,   "NMI/sprite sz/BG base")                                 \
      FLD(PPUCTRL, NMI_EN,      7:7, "NMI enable",                  Flag, 0,0) \
      FLD(PPUCTRL, PPU_SELECT,  6:6, "PPU master/slave",            Flag, 0,0) \
      FLD(PPUCTRL, SPRITE_SZ,   5:5, "Sprite size (1=8x16)",        Flag, 0,0) \
      FLD(PPUCTRL, BG_PT_BASE,  4:4, "BG pattern base (1=$1000)",   Flag, 0,0) \
      FLD(PPUCTRL, SPR_PT_BASE, 3:3, "SPR pattern base (1=$1000)",  Flag, 0,0) \
      FLD(PPUCTRL, VRAM_INC,    2:2, "VRAM increment (1=+32)",      Flag, 0,0) \
      FLD(PPUCTRL, NT_SELECT,   1:0, "Base nametable",              Value,0,0) \
    REG(1, PPUMASK,   "Render/grayscale")                                      \
      FLD(PPUMASK, EMPH_B,      7:7, "Emphasize blue",              Flag, 0,0) \
      FLD(PPUMASK, EMPH_G,      6:6, "Emphasize green",             Flag, 0,0) \
      FLD(PPUMASK, EMPH_R,      5:5, "Emphasize red",               Flag, 0,0) \
      FLD(PPUMASK, SHOW_SPR,    4:4, "Show sprites",                Flag, 0,0) \
      FLD(PPUMASK, SHOW_BG,     3:3, "Show background",             Flag, 0,0) \
      FLD(PPUMASK, LEFT_SPR,    2:2, "Show sprites left 8px",       Flag, 0,0) \
      FLD(PPUMASK, LEFT_BG,     1:1, "Show BG left 8px",            Flag, 0,0) \
      FLD(PPUMASK, GREYSCALE,   0:0, "Greyscale",                   Flag, 0,0) \
    REG(2, PPUSTATUS, "VBlank/spr0/overflow")                                  \
      FLD(PPUSTATUS, VBLANK,    7:7, "In VBlank",                   Flag, 0,0) \
      FLD(PPUSTATUS, SPR0_HIT,  6:6, "Sprite 0 hit",                Flag, 0,0) \
      FLD(PPUSTATUS, SPR_OVF,   5:5, "Sprite overflow",             Flag, 0,0) \
    REG(3, OAMADDR,   "OAM address")                                           \
    REG(4, OAMDATA,   "OAM data R/W")                                          \
    REG(5, PPUSCROLL, "Fine scroll X/Y")                                       \
    REG(6, PPUADDR,   "VRAM address")                                          \
    REG(7, PPUDATA,   "VRAM data R/W")

DECL_EXTRACT(NES_PPU, NES_PPU_DECL)

namespace nes_system {

class PPU : public VideoChipBase {
public:
    // PPU register indices (memory-mapped at $2000-$2007)
    #define NES_PPU_X_CONST_(a, s, l) static constexpr uint8_t s = a;
    NES_PPU_DECL(NES_PPU_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    #undef NES_PPU_X_CONST_
    static constexpr uint8_t REG_COUNT = 8;

    // PPU register file stored in ChipBase::regs_[]

    // PPU memory
    //
    // CIRAM (2KB nametable VRAM) now lives in the flat mem (nes_bus_t).
    // ciram_ is a non-owning pointer set by connect_bus().
    static constexpr size_t CIRAM_SIZE = nes_bus::CIRAM_SIZE;
    uint8_t* ciram_ = nullptr;

    // OAM entry — 4-byte POD mapping directly to NES hardware layout.
    // Used for both primary OAM (64 entries) and secondary OAM (8 entries).
    struct OamEntry {
        uint8_t y;
        uint8_t tile_id;
        uint8_t attributes;
        uint8_t x;
    };
    static_assert(sizeof(OamEntry) == 4, "OamEntry must be exactly 4 bytes");

    // Primary OAM — 64 sprites × 4 bytes = 256 bytes.
    // Union provides both byte-level access (indexed read/write, DMA) and
    // structured access (per-sprite field reads via .entries[]).
    union {
        uint8_t  bytes[256];
        OamEntry entries[64];
    } oam{};

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
        uint16_t bg_pattern_lo_addr = 0; // Cached pattern table low address (case 4→6)

        // Background shift registers
        uint16_t bg_shifter_pattern_lo = 0;
        uint16_t bg_shifter_pattern_hi = 0;
        uint16_t bg_shifter_attrib_lo = 0;
        uint16_t bg_shifter_attrib_hi = 0;

        // Secondary OAM — single buffer.
        // Sprite eval writes during cycles 65-256; commit_sprite_eval() at
        // cycle 257 extracts all rendering data into flat arrays.  After
        // commit, sec_oam_ is untouched until the next scanline's cycle 0 clear.
        union SecOam {
            uint8_t  bytes[32];
            OamEntry entries[8];
        };
        SecOam   sec_oam_;               // Single secondary OAM buffer
        // No double buffering needed: commit_sprite_eval() copies all
        // rendering data (x, attr, pattern addrs) into flat arrays at
        // cycle 257.  After commit, nothing reads sec_oam_ until the
        // next scanline's cycle 0 clear.

        uint8_t sprite_count = 0;                  // Sprites found during evaluation
        uint16_t sprite_pattern_addr[8] = {};          // Cached pattern-table low addresses
        uint8_t sprite_shifter_pattern_lo[8] = {};
        uint8_t sprite_shifter_pattern_hi[8] = {};
        uint8_t sprite_x[8] = {};                      // Original OAM x (read-only after commit)
        uint8_t sprite_attr[8] = {};                    // Cached attributes (populated at commit)

        bool sprite_zero_hit_possible = false;

        // Sprite evaluation state machine — models per-cycle evaluation
        // during dots 65-256 on visible scanlines for accurate overflow
        // flag timing and the PPU's buggy overflow byte-offset behavior.
        //
        // Packed into a single uint16_t `state`:
        //   [15]   = done (also set naturally when n==64)
        //   [14:9] = n  (sprite index, 0-63)
        //   [8:7]  = m  (overflow byte offset, 0-3)
        //   [5]    = overflow phase (0=finding, 1=overflow check)
        //   [4:0]  = sec_wr (secondary OAM write pointer, 0-31)
        //
        // state=0 is a valid cold start (finding, sprite 0, sec_wr=0).
        // sec_wr carry (31→32) naturally sets SE_OVF (finding→overflow).
        // m carry (3→0) naturally increments n.
        // n carry (63→64) naturally sets SE_DONE.
        struct SpriteEval {
            uint16_t state         = 0;  // packed evaluation state (see layout above)
            uint8_t  sprite_height = 8;  // 8 or 16; latched from PPUCTRL at scanline start
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
    // to regs_[PPUSTATUS] bit 7 at the START of the next PPU::clock() call.
    // The same 1-dot delay applies to VBL clear at pre-render dot 1.
    bool     vbl_flag_internal_ = false;    // True = VBL active (drives NMI)
    bool     pending_vbl_set_ = false;      // Commit regs_[PPUSTATUS] |= 0x80 next dot
    bool     pending_vbl_clear_ = false;    // Commit regs_[PPUSTATUS] &= ~0x80 next dot

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

    // Per-scanline color buffer — stores 6-bit NES master color indices.
    // Resolved from palette RAM (via pal_mirror_) at pixel time; flushed
    // to display at cycle 257.  Mid-scanline emphasis changes trigger
    // partial flushes so pixels use the emphasis variant in effect when drawn.
    uint8_t scanline_color_line_[256] = {};
    int     scanline_flush_x_ = 0;          // Next x to flush (0–256)

    // Display output — system-owned IndexedFrameBuffer, registered via
    // set_display().  Replaces the former VideoPixelUnit + screen vector.
    IndexedFrameBuffer* display_ = nullptr;

    // Active palette variant — pointer into palette_cache_[].  Set to nullptr
    // to mark as stale; the render path checks this once per dot with an
    // unlikely branch and reselects the variant on demand.  Coalesces rapid
    // emphasis/mask writes (common during setup) into a single reselection.
    const uint32_t* active_palette_ = nullptr;

public:
    PPU(bool pal = false) : is_pal(pal),
        total_scanlines_minus_one_(pal ? 311 : 261) {  // PAL: 312-1, NTSC: 262-1
        init_regs(REG_COUNT);
        info_ = ChipInfo{pal ? "RP2C07" : "RP2C02", "Ricoh"};
        // Initialize PPU memory (std::array zero-initialized by {})
        std::memset(internal.sec_oam_.bytes, 0xFF, 32);
        internal.sprite_count = 0;

        build_palette_cache(is_pal, palette_cache_);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void reset() {
        std::memset(regs_, 0, num_regs_);
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
        scanline_flush_x_ = 0;
        ppu_dot_count_ = 0;
        vram_data_latch_ = 0;
        std::fill(std::begin(open_bus_refresh_), std::end(open_bus_refresh_), 0);
        io_latch_ = 0;
        bus_snapshot_ = PPU_BUS_DEFAULT_STATE;   // PPU bus (active-low signals HIGH)

        // Clear memory
        if (ciram_) std::memset(ciram_, 0, CIRAM_SIZE);
        std::memset(oam.bytes, 0, sizeof(oam.bytes));
        palette.fill(0);

        build_palette_cache(is_pal, palette_cache_);
        rebuild_active_palette();
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

    // Set the display output (system-owned IndexedFrameBuffer).
    void set_display(IndexedFrameBuffer* d) { display_ = d; }

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

    // Internal rendering functions (defined inline in nes_ppu_clock.inl)
    inline void increment_scroll_x(uint8_t mask);
    inline void increment_scroll_y(uint8_t mask);
    inline void transfer_address_x(uint8_t mask);
    inline void transfer_address_y(uint8_t mask);
    inline void load_background_shifters();
    inline void update_shifters(uint8_t mask);

    // Flush already-rendered pixels [scanline_flush_x_, cycle-1) with the
    // current active_palette_ before an emphasis change invalidates it.
    // Called from service_cpu_bus when a write to $2001 or palette RAM
    // occurs during visible rendering.  No-op outside the visible window.
    inline void flush_scanline_segment() {
        if (scanline < 0 || scanline >= 240) return;
        const int x_end = cycle - 1;  // last rendered pixel = cycle-2, range is [flush_x, cycle-1)
        if (x_end <= scanline_flush_x_) return;
        if (!active_palette_) rebuild_active_palette();
        if (display_) display_->flush_line_range(
            scanline, scanline_color_line_, active_palette_, scanline_flush_x_, x_end);
        scanline_flush_x_ = x_end;
    }

    // Reselect the active emphasis/greyscale palette variant from PPUMASK.
    // Called lazily when active_palette_ is null (after invalidation).
    void rebuild_active_palette() {
        const uint8_t variant = ((regs_[PPUMASK] >> 5) & 0x07) | ((regs_[PPUMASK] & 0x01) << 3);
        active_palette_ = palette_cache_[variant];
    }

    // Packed state field constants for SpriteEval::state
    static constexpr uint16_t SE_DONE       = 0x8000u;  // [15]   done; also set naturally when n==64
    static constexpr uint16_t SE_SPRITE     = 0x7E00u;  // [14:9] primary OAM sprite index (0-63)
    static constexpr uint16_t SE_SPRITE_INC = 0x0200u;  // sprite index unit increment
    static constexpr uint16_t SE_BYTE       = 0x0180u;  // [8:7]  byte offset within sprite (0-3)
    static constexpr uint16_t SE_INC        = 0x0081u;  // byte offset++ and write pointer++ in one add
    static constexpr uint16_t SE_OVF        = 0x0020u;  // [5]    0=finding, 1=overflow check
    static constexpr uint16_t SE_WR         = 0x001Fu;  // [4:0]  write pointer into sec OAM (0-31)
    static constexpr uint8_t  SE_OAM_SHF    = 7;        // state >> SE_OAM_SHF = n*4+m (OAM byte index)
    static constexpr uint8_t  SE_SPRITE_SHF = 9;        // state >> SE_SPRITE_SHF = n   (bitmask index)

    // Sprite evaluation (defined inline in nes_ppu_clock.inl).
    // commit_sprite_eval() swaps the double-buffered secondary OAM
    // and precomputes sprite pattern addresses at cycle 257.
    inline void commit_sprite_eval();
    inline void sprite_eval_step();

public:
    // Write a byte to primary OAM.
    // Public — the system DMA controller writes OAM directly.
    inline void oam_write(uint8_t addr, uint8_t data) {
        oam.bytes[addr] = data;
    }

private:
    // Sprite pattern address calculation — returns the low-byte pattern
    // table address for the given sprite slot.  High byte is addr + 8.
    // Works for all 8 slots: unused slots ($FF OAM) produce valid PPU
    // bus addresses for correct A12 transitions.
    inline uint16_t compute_sprite_pattern_addr(uint8_t i) const {
        const auto& spr = internal.sec_oam_.entries[i];
        if (regs_[PPUCTRL] & 0x20) {
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
            return ((regs_[PPUCTRL] & 0x08) << 9) | (spr.tile_id << 4) | row;
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
    // Uses vbl_flag_internal_ (set at dot 1) rather than regs_[PPUSTATUS] bit 7
    // (visible at dot 2) so NMI asserts at the correct PPU clock.
    inline void update_nmi_output(ppu_bus_state_t& ppu_bus) {
        if (vbl_flag_internal_ && (regs_[PPUCTRL] & 0x80)) {
            PPU_BUS_CLR_BIT(ppu_bus, BUS_NMI_BIT);  // active low = asserted
        } else {
            PPU_BUS_SET_BIT(ppu_bus, BUS_NMI_BIT);  // inactive high
        }
    }

    // --- ChipBase interface ---
public:
#ifdef CERMU_HAS_GUI
    bool has_settings_content() const override;
    void render_settings_content() override;
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
    const char* get_layout_chip_name() const override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};

// Hot-path inline implementations — called 89,342× per NTSC frame.
// Kept in a separate .inl for readability; included here so the compiler
// can inline clock() and helpers into the system tick loop.
#include "chip/video/nes_ppu/nes_ppu_clock.inl"

} // namespace nes_system
