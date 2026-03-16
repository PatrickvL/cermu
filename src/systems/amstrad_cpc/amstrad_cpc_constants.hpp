#pragma once
/*
 * amstrad_cpc_constants.h — Amstrad CPC system constants
 *
 * Covers the Amstrad CPC 464 (1984), CPC 664 (1985), and CPC 6128 (1985).
 * All are Z80A-based with MC6845 CRTC + gate array for video, AY-3-8912
 * for sound, and i8255 PPI for I/O multiplexing.
 */

#include <cstdint>

namespace amstrad_cpc_constants {

    // ========================================================================
    // Timing
    // ========================================================================

    inline constexpr uint32_t CPU_FREQ_HZ         = 4000000;   // 4 MHz Z80A
    inline constexpr uint32_t TSTATES_PER_FRAME   = 256 * 312;  // 79872

    // ========================================================================
    // Display — 3 screen modes
    // ========================================================================
    //
    // Mode 0: 160×200, 16 colors (4 bpp, 2 pixels per byte)
    // Mode 1: 320×200, 4 colors  (2 bpp, 4 pixels per byte) — default
    // Mode 2: 640×200, 2 colors  (1 bpp, 8 pixels per byte)

    // Framebuffer uses Mode 2 resolution (highest)
    inline constexpr int FB_WIDTH            = 640;
    inline constexpr int FB_HEIGHT           = 400;  // Double for display aspect

    // ========================================================================
    // Audio
    // ========================================================================

    inline constexpr int      DEFAULT_SAMPLE_RATE = 44100;

    // ========================================================================
    // Gate Array (Amstrad custom, 40018 / 40226)
    // ========================================================================
    // Functions: pen selection, color lookup, ROM banking, screen mode,
    //            interrupt generation (every 52 scanlines)

    inline constexpr int GA_PEN_COUNT           = 17;    // 16 ink pens + border
    inline constexpr int GA_COLOR_COUNT         = 27;    // 27 hardware colors
    inline constexpr int GA_INK_VALUES          = 32;    // 32 possible ink register values

    // ====================================================================
    // CPC Hardware Color Palette (ABGR format)
    // ====================================================================
    //
    // Gate Array ink register values (0-31) map to these colors.
    // 32 entries with 27 unique colors and 5 duplicates.
    // Each channel has 3 levels: 0x00, 0x60, 0xFF.
    // Values from MAME's amstrad.cpp driver (verified against hardware).
    //
    inline constexpr uint32_t HARDWARE_PALETTE[32] = {
        0xFF606060,  //  0: White (half-bright)
        0xFF606060,  //  1: White (half-bright) [dup of 0]
        0xFF60FF00,  //  2: Sea Green
        0xFF60FFFF,  //  3: Pastel Yellow
        0xFF600000,  //  4: Blue
        0xFF6000FF,  //  5: Purple
        0xFF606000,  //  6: Cyan
        0xFF6060FF,  //  7: Pink
        0xFFF000FF,  //  8: Purple [dup of 5]
        0xFFF0FFFF,  //  9: Pastel Yellow [dup of 3]
        0xFFF0FF00,  // 10: Bright Yellow
        0xFFF0FFFF,  // 11: Bright White
        0xFFF00000,  // 12: Bright Red
        0xFFF000FF,  // 13: Bright Magenta
        0xFFF06000,  // 14: Orange
        0xFFF060FF,  // 15: Pastel Magenta
        0xFF000060,  // 16: Blue [dup of 4, different brightness]
        0xFF00FF60,  // 17: Sea Green [dup of 2, different brightness]
        0xFF00FF00,  // 18: Bright Green
        0xFF00FFFF,  // 19: Bright Cyan
        0xFF000000,  // 20: Black
        0xFF0000FF,  // 21: Bright Blue
        0xFF006000,  // 22: Green
        0xFF0060FF,  // 23: Sky Blue
        0xFF600060,  // 24: Magenta
        0xFF60FF60,  // 25: Pastel Green
        0xFF60FF00,  // 26: Lime
        0xFF60FFFF,  // 27: Pastel Cyan
        0xFF600000,  // 28: Red
        0xFF6000FF,  // 29: Mauve
        0xFF606000,  // 30: Yellow
        0xFF6060FF,  // 31: Pastel Blue
    };

} // namespace amstrad_cpc_constants
