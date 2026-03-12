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

} // namespace amstrad_cpc_constants
