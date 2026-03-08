#pragma once
/*
 * spectrum_constants.h — ZX Spectrum system constants
 *
 * Covers the ZX Spectrum 48K (1982) and ZX Spectrum 128K (1985).
 * Both are Z80A-based, differing primarily in memory, AY sound, and banking.
 */

#include <cstdint>

namespace spectrum_constants {

    // ========================================================================
    // Timing (PAL only — the Spectrum was never NTSC)
    // ========================================================================

    inline constexpr uint32_t CPU_FREQ_HZ         = 3500000;   // 3.5 MHz Z80A
    inline constexpr int      TSTATES_PER_LINE    = 224;
    inline constexpr int      SCANLINES_PER_FRAME = 312;
    inline constexpr uint32_t TSTATES_PER_FRAME   = TSTATES_PER_LINE * SCANLINES_PER_FRAME;  // 69888
    inline constexpr float    FPS                  = 50.08f;    // 3500000 / 69888

    // ========================================================================
    // Display
    // ========================================================================

    inline constexpr int SCREEN_WIDTH              = 256;
    inline constexpr int SCREEN_HEIGHT             = 192;
    inline constexpr int BORDER_LEFT               = 48;
    inline constexpr int BORDER_RIGHT              = 48;
    inline constexpr int BORDER_TOP                = 48;
    inline constexpr int BORDER_BOTTOM             = 56;
    inline constexpr int TOTAL_WIDTH               = BORDER_LEFT + SCREEN_WIDTH + BORDER_RIGHT;
    inline constexpr int TOTAL_HEIGHT              = BORDER_TOP + SCREEN_HEIGHT + BORDER_BOTTOM;

    // ========================================================================
    // Memory Map — 48K
    // ========================================================================
    //
    //   $0000-$3FFF: 16KB ROM (Spectrum 48K BASIC)
    //   $4000-$5AFF: 6912 bytes screen bitmap (256×192 = 6144) + attributes (768)
    //   $5B00-$FFFF: User RAM
    //
    // Memory Map — 128K (paged)
    //   $0000-$3FFF: ROM bank (ROM 0 = 128K editor, ROM 1 = 48K BASIC)
    //   $4000-$7FFF: RAM bank 5 (always, contains display file)
    //   $8000-$BFFF: RAM bank 2 (always)
    //   $C000-$FFFF: Switchable RAM bank (0-7, selected via port $7FFD)

    inline constexpr uint16_t ROM_BASE             = 0x0000;
    inline constexpr uint16_t ROM_SIZE_48K         = 0x4000;   // 16KB
    inline constexpr uint16_t SCREEN_BASE          = 0x4000;
    inline constexpr uint16_t SCREEN_BITMAP_SIZE   = 6144;
    inline constexpr uint16_t SCREEN_ATTR_SIZE     = 768;
    inline constexpr uint16_t SCREEN_TOTAL_SIZE    = SCREEN_BITMAP_SIZE + SCREEN_ATTR_SIZE;
    inline constexpr uint16_t USER_RAM_BASE_48K    = 0x5B00;

    // 128K banking port
    inline constexpr uint16_t BANK_SELECT_PORT     = 0x7FFD;
    inline constexpr uint16_t RAM_BANK_SIZE        = 0x4000;   // 16KB per bank
    inline constexpr int      RAM_BANK_COUNT_128K  = 8;        // 128KB = 8 × 16KB

    // ========================================================================
    // I/O Ports
    // ========================================================================

    inline constexpr uint16_t ULA_PORT             = 0xFE;     // A0=0 selects ULA
    inline constexpr uint16_t AY_REG_PORT          = 0xFFFD;   // AY register select (128K)
    inline constexpr uint16_t AY_DATA_PORT         = 0xBFFD;   // AY data write (128K)

    // ========================================================================
    // Audio
    // ========================================================================

    inline constexpr uint32_t AY_CLOCK_HZ          = CPU_FREQ_HZ / 2;  // 1.75 MHz
    inline constexpr int      DEFAULT_SAMPLE_RATE   = 44100;

    // ========================================================================
    // Keyboard
    // ========================================================================
    // 8 half-rows × 5 keys, active-low, selected by address lines A8-A15

    inline constexpr int KEYBOARD_ROWS             = 8;
    inline constexpr int KEYS_PER_ROW              = 5;

} // namespace spectrum_constants
