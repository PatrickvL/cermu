#pragma once
/*
 * mc6845.h — Motorola MC6845 CRT Controller (CRTC)
 *
 * Generic implementation of the MC6845 (and pin-compatible variants:
 * Hitachi HD46505, UMC UM6845, Rockwell 6545, Synertek SY6545).
 *
 * The MC6845 generates timing and addressing for raster-scanned CRT displays.
 * It does NOT produce pixel data — it provides row/column addresses and sync
 * signals.  The host system combines these with character ROM lookup and
 * attribute RAM to produce the display.
 *
 * Used in: Commodore PET/CBM, BBC Micro, Amstrad CPC, many S-100 systems.
 *
 * Register map (accent on PET-relevant subset):
 *   R0  — Horizontal Total         (chars per line - 1)
 *   R1  — Horizontal Displayed     (visible chars per line)
 *   R2  — Horizontal Sync Position (char count when HSYNC starts)
 *   R3  — Sync Widths              ([3:0] = HSYNC width, [7:4] = VSYNC width)
 *   R4  — Vertical Total           (char rows per frame - 1)
 *   R5  — Vertical Adjust          (extra scan lines for fine frame timing)
 *   R6  — Vertical Displayed       (visible char rows)
 *   R7  — Vertical Sync Position   (char row when VSYNC starts)
 *   R8  — Mode Control             (interlace/skew)
 *   R9  — Max Scan Line Address    (scan lines per char row - 1)
 *   R10 — Cursor Start             (scan line + blink mode)
 *   R11 — Cursor End               (scan line)
 *   R12 — Start Address High       (display start address MSB)
 *   R13 — Start Address Low        (display start address LSB)
 *   R14 — Cursor Address High      (cursor position MSB)
 *   R15 — Cursor Address Low       (cursor position LSB)
 *   R16 — Light Pen High (read-only)
 *   R17 — Light Pen Low  (read-only)
 *
 * I/O interface: two registers at base+0 (address) and base+1 (data).
 */

#include "../core/chip.h"
#include <cstdint>
#include <functional>

// ============================================================================
// MC6845 REGISTER INDICES
// ============================================================================

static constexpr uint8_t MC6845_R0_HTOTAL          = 0;
static constexpr uint8_t MC6845_R1_HDISPLAYED      = 1;
static constexpr uint8_t MC6845_R2_HSYNC_POS       = 2;
static constexpr uint8_t MC6845_R3_SYNC_WIDTHS     = 3;
static constexpr uint8_t MC6845_R4_VTOTAL          = 4;
static constexpr uint8_t MC6845_R5_VADJUST         = 5;
static constexpr uint8_t MC6845_R6_VDISPLAYED      = 6;
static constexpr uint8_t MC6845_R7_VSYNC_POS       = 7;
static constexpr uint8_t MC6845_R8_MODE_CTRL       = 8;
static constexpr uint8_t MC6845_R9_MAX_SCANLINE    = 9;
static constexpr uint8_t MC6845_R10_CURSOR_START   = 10;
static constexpr uint8_t MC6845_R11_CURSOR_END     = 11;
static constexpr uint8_t MC6845_R12_START_ADDR_HI  = 12;
static constexpr uint8_t MC6845_R13_START_ADDR_LO  = 13;
static constexpr uint8_t MC6845_R14_CURSOR_HI      = 14;
static constexpr uint8_t MC6845_R15_CURSOR_LO      = 15;
static constexpr uint8_t MC6845_R16_LPEN_HI        = 16;
static constexpr uint8_t MC6845_R17_LPEN_LO        = 17;

static constexpr int MC6845_NUM_REGISTERS = 18;

// ============================================================================
// MC6845 CHIP STRUCTURE
// ============================================================================

struct mc6845_t : public ChipBase {
    mc6845_t() : ChipBase(ChipInfo{"MC6845", "Motorola"}) {}

    // ========================================================================
    // REGISTERS
    // ========================================================================

    uint8_t address_register = 0;               // Currently selected register
    uint8_t regs[MC6845_NUM_REGISTERS] = {};    // R0–R17

    // ========================================================================
    // COUNTERS (runtime state)
    // ========================================================================

    // Horizontal timing
    uint8_t h_char_counter = 0;         // Character counter within line (0 to R0)
    uint8_t h_sync_counter = 0;         // HSYNC duration counter
    bool    h_sync_active = false;      // HSYNC output
    bool    h_display_active = false;   // Horizontal display enable

    // Vertical timing
    uint8_t v_row_counter = 0;          // Character row counter (0 to R4)
    uint8_t v_scanline_counter = 0;     // Scan line counter within row (0 to R9)
    uint8_t v_adjust_counter = 0;       // Vertical adjust scan line counter (0 to R5)
    uint8_t v_sync_counter = 0;         // VSYNC duration counter
    bool    v_sync_active = false;      // VSYNC output
    bool    v_display_active = false;   // Vertical display enable
    bool    in_adjust = false;          // Currently in vertical adjust phase

    // Display address
    uint16_t linear_address = 0;        // Current display memory address (MA0–MA13)
    uint16_t row_start_address = 0;     // Address at start of current character row

    // Cursor
    uint8_t cursor_blink_counter = 0;   // Frame counter for cursor blink
    bool    cursor_visible = true;      // Current cursor visibility (after blink logic)

    // Light pen
    bool    light_pen_latched = false;  // Light pen position has been captured
    uint16_t light_pen_address = 0;     // Captured light pen address

    // Frame counter (for blink timing)
    uint32_t frame_count = 0;

    // ========================================================================
    // CALLBACKS — set by the host system
    // ========================================================================

    // Called once per character clock during the active display area.
    // Provides: character address (MA), scan line within character (RA),
    // cursor active flag.
    // The host system uses this to look up screen RAM + character ROM and
    // render pixels.
    using display_callback_t = std::function<void(uint16_t ma, uint8_t ra, bool cursor)>;
    display_callback_t on_display_char = nullptr;

    // Called at the start of each new frame (VSYNC rising edge).
    using vsync_callback_t = std::function<void()>;
    vsync_callback_t on_vsync = nullptr;

    // Called at the start of each new scan line (including blanked lines).
    using hsync_callback_t = std::function<void()>;
    hsync_callback_t on_hsync = nullptr;

    // ========================================================================
    // INTERFACE
    // ========================================================================

    /// Initialize/reset the CRTC to power-on state.
    void init();
    void reset();

    /// Register access (memory-mapped: addr bit 0 selects address vs data).
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t data);

    /// Advance the CRTC by one character clock.
    /// Call this once per character clock (typically 1 MHz on PET).
    void tick();

    // --- Derived state queries ------------------------------------------

    /// Total characters per line (R0 + 1).
    inline uint8_t chars_per_line() const { return regs[MC6845_R0_HTOTAL] + 1; }

    /// Visible characters per line (R1).
    inline uint8_t visible_chars() const { return regs[MC6845_R1_HDISPLAYED]; }

    /// Total character rows per frame (R4 + 1).
    inline uint8_t rows_per_frame() const { return regs[MC6845_R4_VTOTAL] + 1; }

    /// Visible character rows (R6).
    inline uint8_t visible_rows() const { return regs[MC6845_R6_VDISPLAYED]; }

    /// Scan lines per character row (R9 + 1).
    inline uint8_t scanlines_per_row() const { return regs[MC6845_R9_MAX_SCANLINE] + 1; }

    /// Display start address from R12:R13.
    inline uint16_t start_address() const {
        return (static_cast<uint16_t>(regs[MC6845_R12_START_ADDR_HI]) << 8) |
               regs[MC6845_R13_START_ADDR_LO];
    }

    /// Cursor address from R14:R15.
    inline uint16_t cursor_address() const {
        return (static_cast<uint16_t>(regs[MC6845_R14_CURSOR_HI]) << 8) |
               regs[MC6845_R15_CURSOR_LO];
    }

    /// Is the display currently in the active (visible) area?
    inline bool is_display_active() const {
        return h_display_active && v_display_active;
    }
};
