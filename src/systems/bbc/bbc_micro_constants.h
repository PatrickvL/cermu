#pragma once

#include <cstdint>

// ============================================================================
// BBC MICRO MODEL B SYSTEM CONSTANTS
// ============================================================================
//
// The BBC Micro Model B (1981, Acorn Computers) uses:
//   CPU:   MOS 6502A @ 2 MHz
//   RAM:   32 KB ($0000-$7FFF)
//   VIAs:  2x MOS 6522 (System VIA + User VIA)
//   CRTC:  MC6845 (Motorola) — generates display timing/addressing
//   Sound: SN76489 (Texas Instruments) — 3 tone + 1 noise channel
//   Video: Video ULA (custom) — maps CRTC addresses to pixel modes
//
// Memory map:
//   $0000-$7FFF  32 KB RAM
//   $8000-$BFFF  16 KB paged ROM (sideways ROM/RAM bank)
//   $C000-$FBFF  16 KB OS ROM (MOS)
//   $FC00-$FCFF  FRED   — 1 MHz I/O bus page (active-low accent)
//   $FD00-$FDFF  JIM    — 1 MHz I/O bus page
//   $FE00-$FEFF  SHEILA — system I/O page
//   $FF00-$FFFF  OS ROM (vectors, top of MOS)
//
// SHEILA I/O page breakdown ($FE00-$FEFF):
//   $FE00-$FE07  MC6845 CRTC (address at even, data at odd)
//   $FE08-$FE0F  MC6850 ACIA (serial — optional, accent accent serial)
//   $FE10-$FE1F  Serial ULA
//   $FE20-$FE2F  Video ULA (control + palette)
//   $FE30-$FE3F  Paged ROM select register
//   $FE40-$FE5F  System VIA (MOS 6522)
//   $FE60-$FE7F  User VIA (MOS 6522)
//   $FE80-$FE9F  Floppy disc controller (8271 or 1770)
//   $FEA0-$FEBF  Econet (68B54 ADLC — optional)
//   $FEC0-$FEDF  ADC (uPD7002 — analog input)
//   $FEE0-$FEFF  Tube ULA (second processor interface — optional)

namespace bbc_constants {

    // CPU / timing
    // BBC Micro master clock is 16 MHz.  CPU runs at 2 MHz (master/8).
    // The CRTC character clock is 1 MHz (master/16) in modes 0-3,
    // or 2 MHz in modes 4-7 (teletext modes use 1 MHz effective).
    inline constexpr uint32_t MASTER_CLOCK          = 16000000;
    inline constexpr uint32_t CPU_FREQ              = 2000000;   // 2 MHz
    inline constexpr uint32_t SN76489_CLOCK         = 250000;    // 4 MHz / 16 internal divider
    inline constexpr uint32_t TARGET_FPS            = 50;        // PAL
    inline constexpr uint32_t CYCLES_PER_FRAME      = CPU_FREQ / TARGET_FPS;  // 40000

    // Memory sizes
    inline constexpr uint32_t RAM_SIZE              = 32768;     // 32 KB
    inline constexpr uint32_t OS_ROM_SIZE           = 16384;     // 16 KB MOS
    inline constexpr uint32_t PAGED_ROM_SIZE        = 16384;     // 16 KB per sideways ROM slot

    // SHEILA I/O page ($FE00-$FEFF)
    inline constexpr uint16_t SHEILA_START          = 0xFE00;
    inline constexpr uint16_t SHEILA_END            = 0xFEFF;

    // SHEILA sub-ranges
    inline constexpr uint16_t CRTC_BASE             = 0xFE00;    // $FE00-$FE07
    inline constexpr uint16_t CRTC_END              = 0xFE07;
    inline constexpr uint16_t ROM_SELECT_REG        = 0xFE30;    // $FE30 — paged ROM bank select
    inline constexpr uint16_t SYSTEM_VIA_BASE       = 0xFE40;    // $FE40-$FE5F
    inline constexpr uint16_t SYSTEM_VIA_END        = 0xFE5F;
    inline constexpr uint16_t USER_VIA_BASE         = 0xFE60;    // $FE60-$FE7F
    inline constexpr uint16_t USER_VIA_END          = 0xFE7F;

    // FRED / JIM I/O pages (active-low accent on the 1 MHz bus)
    inline constexpr uint16_t FRED_START            = 0xFC00;

    // Display
    // Mode 7 (Teletext): 40×25 characters — simplified text display
    // Modes 0-6: bitmap modes of various resolutions
    // We start with Mode 7 (text) as the default boot mode
    inline constexpr uint32_t MODE7_COLS            = 40;
    inline constexpr uint32_t MODE7_ROWS            = 25;

    // Bitmap modes: max is Mode 0 (640×256, 2 colors)
    inline constexpr uint32_t DISPLAY_WIDTH         = 640;
    inline constexpr uint32_t DISPLAY_HEIGHT        = 256;

    // Default audio sample rate
    inline constexpr uint32_t DEFAULT_SAMPLE_RATE   = 44100;

    // Video ULA registers (written at $FE20-$FE21)
    // $FE20: Control register
    //   bits 6-4: characters per line (determines pixel width)
    //   bit 3: clock rate (0 = 1 MHz CRTC, 1 = 2 MHz CRTC)
    //   bit 2: 6845 cursor width (not normally used)
    //   bits 1-0: flash period
    // $FE21: Palette register (maps logical color → physical color)
    inline constexpr uint16_t VIDEO_ULA_CONTROL     = 0xFE20;
    inline constexpr uint16_t VIDEO_ULA_PALETTE     = 0xFE21;

    // System VIA Port A bits (accent accent accent accent accent accent)
    // PA0-PA2: keyboard column select (active-low)
    // PA3:     slow data bus D0 (active-low accent accent)
    // PA4:     slow data bus D1
    // PA5:     slow data bus D2
    // PA6:     slow data bus D3
    // PA7:     sound chip /WE (active-low; accent accent accent accent)

    // System VIA Port B bits
    // PB0-PB2: addressable latch select (accent accent accent)
    // PB3:     addressable latch data
    // PB4:     CRTC light pen strobe
    // PB5:     unused
    // PB6:     unused
    // PB7:     VSYNC input (active-high when VSYNC)

    // Keyboard matrix: 10 columns × 8 rows
    // System VIA Port A [bits 6:0] scans one combination at a time.
    // CA2 selects keyboard vs. sound chip.
    inline constexpr uint32_t KEYBOARD_COLS         = 10;
    inline constexpr uint32_t KEYBOARD_ROWS         = 8;

    // CRTC register values for Mode 7 (Teletext, 40×25 text)
    // These are the values the MOS ROM programs at boot for Mode 7.
    inline constexpr uint8_t MODE7_CRTC_REGS[] = {
        63,     // R0:  Horizontal Total (64 chars per line)
        40,     // R1:  Horizontal Displayed (40 visible chars)
        51,     // R2:  H sync position
        0x34,   // R3:  Sync widths (H=4, V=3)
        30,     // R4:  Vertical Total (31 char rows)
        2,      // R5:  Vertical Adjust
        25,     // R6:  Vertical Displayed (25 visible rows)
        27,     // R7:  V sync position
        0x00,   // R8:  Mode control (non-interlaced)
        18,     // R9:  Max scan line (19 scan lines per row)
        0x60,   // R10: Cursor start (blink, line 0)
        0x09,   // R11: Cursor end (line 9)
        0x00,   // R12: Start address high
        0x00,   // R13: Start address low
    };

} // namespace bbc_constants
