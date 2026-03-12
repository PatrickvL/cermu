#pragma once
/*
 * atari2600_constants.h — Atari 2600 system constants
 *
 * The Atari 2600 (1977) uses:
 *   - MOS 6507 CPU (6502 with 13-bit address bus, 8KB address space)
 *   - TIA (Television Interface Adapter) — video + audio
 *   - PIA 6532 RIOT — 128 bytes RAM + I/O ports + timer
 *   - ROM cartridge (2KB, 4KB, 8KB with bank switching, or larger)
 */

#include <cstdint>

namespace atari2600_constants {

    // ========================================================================
    // Timing
    // ========================================================================

    // TIA color clock = 3.579545 MHz (NTSC)
    inline constexpr uint32_t TIA_FREQ_NTSC        = 3579545;
    inline constexpr uint32_t TIA_FREQ_PAL          = 3546894;

    // CPU clock = TIA / 3 ≈ 1.193182 MHz (NTSC)
    inline constexpr uint32_t CPU_FREQ_NTSC         = TIA_FREQ_NTSC / 3;   // 1,193,181 Hz
    inline constexpr uint32_t CPU_FREQ_PAL           = TIA_FREQ_PAL  / 3;   // 1,182,298 Hz

    // Frame timing
    inline constexpr int SCANLINES_PER_FRAME_NTSC   = 262;
    inline constexpr int SCANLINES_PER_FRAME_PAL     = 312;
    inline constexpr int CPU_CYCLES_PER_SCANLINE     = 76;

    inline constexpr uint32_t CYCLES_PER_FRAME_NTSC = SCANLINES_PER_FRAME_NTSC * CPU_CYCLES_PER_SCANLINE;  // 19912
    inline constexpr uint32_t CYCLES_PER_FRAME_PAL   = SCANLINES_PER_FRAME_PAL  * CPU_CYCLES_PER_SCANLINE;  // 23712

    inline constexpr float FPS_NTSC                  = 59.922743f;
    inline constexpr float FPS_PAL                    = 49.860759f;

    // ========================================================================
    // Display
    // ========================================================================

    inline constexpr int DISPLAY_WIDTH               = 160;
    inline constexpr int DISPLAY_HEIGHT              = 192;   // Typical visible area (228 total, varies by game)
    // Most games use scanlines ~40-232 for visible content, giving ~192 lines.
    // The system provides a 160×262 framebuffer; the GUI clips to visible area.
    inline constexpr int FB_HEIGHT                   = 262;   // Full frame height (for scanline-accurate rendering)

    // ========================================================================
    // Memory Map (6507: 13-bit address, $0000-$1FFF)
    // ========================================================================
    //
    // Address decoding uses very few chip-select lines:
    //   A12=0, A7=0         → TIA registers ($0000-$007F, mirrors every 64 bytes)
    //   A12=0, A7=1, A9=0   → RIOT RAM ($0080-$00FF, 128 bytes)
    //   A12=0, A7=1, A9=1   → RIOT I/O & Timer ($0280-$02FF)
    //   A12=1                → Cartridge ROM ($1000-$1FFF, 4KB window)
    //
    // The full address space mirrors heavily due to incomplete decoding.

    inline constexpr uint16_t TIA_BASE               = 0x0000;
    inline constexpr uint16_t TIA_MASK               = 0x007F;   // 7 bits for TIA
    inline constexpr uint16_t RIOT_RAM_BASE          = 0x0080;
    inline constexpr uint16_t RIOT_RAM_MASK          = 0x007F;   // 128 bytes
    inline constexpr uint16_t RIOT_IO_BASE           = 0x0280;
    inline constexpr uint16_t RIOT_IO_MASK           = 0x001F;   // 5 bits for RIOT I/O
    inline constexpr uint16_t CART_ROM_BASE          = 0x1000;
    inline constexpr uint16_t CART_ROM_SIZE_2K       = 0x0800;
    inline constexpr uint16_t CART_ROM_SIZE_4K       = 0x1000;

    // ========================================================================
    // Audio
    // ========================================================================

    inline constexpr int DEFAULT_SAMPLE_RATE         = 44100;

    // ========================================================================
    // RIOT Port B bits (active-low console switches)
    // ========================================================================

    inline constexpr uint8_t SWCHB_RESET             = 0x01;   // P0 difficulty is bit 6
    inline constexpr uint8_t SWCHB_SELECT            = 0x02;   // P1 difficulty is bit 7
    inline constexpr uint8_t SWCHB_BW                = 0x08;   // Color/B&W switch
    inline constexpr uint8_t SWCHB_P0_DIFF           = 0x40;   // Player 0 difficulty (A=1, B=0)
    inline constexpr uint8_t SWCHB_P1_DIFF           = 0x80;   // Player 1 difficulty (A=1, B=0)

} // namespace atari2600_constants
