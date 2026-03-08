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
    inline constexpr int      SCANLINES_PER_FRAME = 312;       // PAL
    inline constexpr int      CHARS_PER_LINE      = 64;        // 64 × 1 µs = 64 µs per line
    inline constexpr int      TSTATES_PER_LINE    = 256;       // 64 chars × 4 T-states
    inline constexpr uint32_t TSTATES_PER_FRAME   = TSTATES_PER_LINE * SCANLINES_PER_FRAME;  // 79872
    inline constexpr float    FPS                  = 50.08f;

    // ========================================================================
    // Display — 3 screen modes
    // ========================================================================
    //
    // Mode 0: 160×200, 16 colors (4 bpp, 2 pixels per byte)
    // Mode 1: 320×200, 4 colors  (2 bpp, 4 pixels per byte) — default
    // Mode 2: 640×200, 2 colors  (1 bpp, 8 pixels per byte)

    inline constexpr int SCREEN_WIDTH_MODE0  = 160;
    inline constexpr int SCREEN_WIDTH_MODE1  = 320;
    inline constexpr int SCREEN_WIDTH_MODE2  = 640;
    inline constexpr int SCREEN_HEIGHT       = 200;

    // Framebuffer uses Mode 2 resolution (highest)
    inline constexpr int FB_WIDTH            = 640;
    inline constexpr int FB_HEIGHT           = 400;  // Double for display aspect

    // ========================================================================
    // Memory Map
    // ========================================================================
    //
    // 64KB address space, fully bank-switched by Gate Array:
    //   $0000-$3FFF: Lower ROM (BIOS) / RAM (switchable)
    //   $4000-$7FFF: RAM
    //   $8000-$BFFF: RAM
    //   $C000-$FFFF: Upper ROM (BASIC/AMSDOS) / RAM (switchable)
    //
    // CPC 6128: 128KB RAM in 4 × 16KB banks, configurable mapping

    inline constexpr int RAM_SIZE_464        = 64 * 1024;
    inline constexpr int RAM_SIZE_6128       = 128 * 1024;
    inline constexpr int ROM_SIZE            = 16 * 1024;   // Per ROM (lower + upper)

    // Video RAM: 16KB at configurable base (default screen at $C000 in RAM)
    inline constexpr int VRAM_SIZE           = 16 * 1024;

    // ========================================================================
    // I/O Ports (active accent notation; accent means active-low accent on chip select)
    // ========================================================================
    //   Gate Array:   $7Fxx (active when A15=0, accent bit 14 = high)
    //   CRTC:         $BCxx/$BDxx (A14=0 selects CRTC)
    //   PPI 8255:     $F4xx-$F7xx (active low A11)
    //   AY-3-8912:    Via PPI Port A (data) and Port C (BDIR/BC1 control)
    //   FDC:          $FAxx/$FBxx (CPC 664/6128 with disc)

    inline constexpr uint16_t GATE_ARRAY_PORT   = 0x7F00;
    inline constexpr uint16_t CRTC_REG_PORT     = 0xBC00;  // Write: register select
    inline constexpr uint16_t CRTC_DATA_PORT    = 0xBD00;  // Write: register data
    inline constexpr uint16_t PPI_PORT_A        = 0xF400;
    inline constexpr uint16_t PPI_PORT_B        = 0xF500;
    inline constexpr uint16_t PPI_PORT_C        = 0xF600;
    inline constexpr uint16_t PPI_CONTROL       = 0xF700;

    // ========================================================================
    // Audio
    // ========================================================================

    inline constexpr uint32_t AY_CLOCK_HZ       = 1000000;   // 1 MHz (CPU_FREQ / 4)
    inline constexpr int      DEFAULT_SAMPLE_RATE = 44100;

    // ========================================================================
    // Gate Array (Amstrad custom, 40018 / 40226)
    // ========================================================================
    // Functions: pen selection, color lookup, ROM banking, screen mode,
    //            interrupt generation (every 52 scanlines)

    inline constexpr int GA_PEN_COUNT           = 17;    // 16 ink pens + border
    inline constexpr int GA_COLOR_COUNT         = 27;    // 27 hardware colors

} // namespace amstrad_cpc_constants
