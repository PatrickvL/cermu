#pragma once
/*
 * apple_ii_constants.h — Apple II family system constants
 *
 * Covers the Apple II (1977), Apple IIe (1983), and Apple IIc (1984).
 * All are 6502/65C02-based with soft-switch video (no dedicated video chip).
 *
 * Apple II:    MOS 6502 @ 1.023 MHz, 48KB RAM max, Integer BASIC
 * Apple IIe:   CMOS 65C02 @ 1.023 MHz, 128KB (aux memory), Applesoft BASIC
 * Apple IIc:   CMOS 65C02 @ 1.023 MHz, 128KB, built-in 5.25" floppy
 *
 * Video is generated entirely via soft switches and memory scanning —
 * no dedicated video chip.  Text mode: 40×24 or 80×24 (IIe/IIc).
 * Lo-res: 40×48, 16 colors.  Hi-res: 280×192, 6 effective colors.
 * Double hi-res (IIe/IIc): 560×192.
 */

#include <cstdint>

namespace apple_ii_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
// 14.31818 MHz master / 14 = 1.0227 MHz (NTSC color burst / 14)
inline constexpr uint32_t CPU_FREQ_HZ           = 1022727;

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint32_t RAM_48K               = 49152;    // Apple II max
inline constexpr uint32_t RAM_64K               = 65536;    // Apple IIe/IIc base
inline constexpr uint32_t RAM_128K              = 131072;   // Apple IIe/IIc with aux

// ROM sizes
inline constexpr uint16_t ROM_SIZE_12K          = 0x3000;   // Apple II: $D000-$FFFF
inline constexpr uint16_t ROM_SIZE_16K          = 0x4000;   // Apple IIe/IIc: $C000-$FFFF

// ── Soft Switches ───────────────────────────────────────────────────────
// All video, memory banking, and I/O is controlled via soft switches
// at $C000-$C0FF.  No dedicated I/O chips on the main board.
inline constexpr uint16_t SOFTSW_START          = 0xC000;
inline constexpr uint16_t SOFTSW_END            = 0xC0FF;
inline constexpr uint16_t SLOT_IO_START         = 0xC100;   // Expansion slot I/O ($C100-$C7FF)
inline constexpr uint16_t SLOT_ROM_START        = 0xC800;   // Expansion slot ROM ($C800-$CFFF)

// Key soft switch addresses
inline constexpr uint16_t KBD_DATA              = 0xC000;   // Keyboard data (bit 7 = strobe)
inline constexpr uint16_t KBD_STROBE_CLR        = 0xC010;   // Clear keyboard strobe
inline constexpr uint16_t SPKR_TOGGLE           = 0xC030;   // Speaker toggle
inline constexpr uint16_t TXTCLR                = 0xC050;   // Graphics mode
inline constexpr uint16_t TXTSET                = 0xC051;   // Text mode
inline constexpr uint16_t MIXCLR                = 0xC052;   // Full-screen graphics
inline constexpr uint16_t MIXSET                = 0xC053;   // Mixed text+graphics
inline constexpr uint16_t LOWSCR                = 0xC054;   // Page 1
inline constexpr uint16_t HISCR                 = 0xC055;   // Page 2
inline constexpr uint16_t LORES                 = 0xC056;   // Lo-res graphics
inline constexpr uint16_t HIRES                 = 0xC057;   // Hi-res graphics

// ── Display ─────────────────────────────────────────────────────────────
// Text mode: 40 columns × 24 rows (7×8 pixel characters = 280×192)
inline constexpr int TEXT_COLS                   = 40;
inline constexpr int TEXT_ROWS                   = 24;
inline constexpr int FB_WIDTH                    = 280;
inline constexpr int FB_HEIGHT                   = 192;
// Display output includes overscan borders
inline constexpr int DISPLAY_WIDTH               = 280;
inline constexpr int DISPLAY_HEIGHT              = 192;

// ── Video RAM ───────────────────────────────────────────────────────────
inline constexpr uint16_t TEXT_PAGE1             = 0x0400;   // Text/Lo-res page 1
inline constexpr uint16_t TEXT_PAGE2             = 0x0800;   // Text/Lo-res page 2
inline constexpr uint16_t HIRES_PAGE1           = 0x2000;   // Hi-res page 1
inline constexpr uint16_t HIRES_PAGE2           = 0x4000;   // Hi-res page 2

// ── Timing ──────────────────────────────────────────────────────────────
// NTSC: 262 scanlines × 65 cycles/line = 17030 cycles/frame
inline constexpr int SCANLINES_PER_FRAME        = 262;
inline constexpr int CYCLES_PER_SCANLINE        = 65;
inline constexpr int CYCLES_PER_FRAME           = SCANLINES_PER_FRAME * CYCLES_PER_SCANLINE;

// ── Audio ───────────────────────────────────────────────────────────────
// 1-bit speaker toggle at $C030 — no audio chip
inline constexpr int DEFAULT_SAMPLE_RATE        = 44100;

// ── Palette (16 colors) ─────────────────────────────────────────────────
// Apple II NTSC artifact colors (standard set)
inline constexpr uint32_t PALETTE[16] = {
    0xFF000000,  //  0: Black
    0xFF006A3A,  //  1: Dark green (Magenta complement)
    0xFF002CCA,  //  2: Dark blue
    0xFF0096FF,  //  3: Medium blue (Purple)
    0xFF4D1C00,  //  4: Dark brown
    0xFF808080,  //  5: Grey 1 (Dark grey)
    0xFFFF2997,  //  6: Medium blue-2
    0xFFFFACBF,  //  7: Light blue
    0xFF004C00,  //  8: Dark green-2
    0xFF00E600,  //  9: Green
    0xFF808080,  // 10: Grey 2 (Light grey)
    0xFF66FFD9,  // 11: Aquamarine
    0xFF2FBF00,  // 12: Orange (Green complement)
    0xFFE6FF80,  // 13: Yellow
    0xFFFF6FE6,  // 14: Pink
    0xFFFFFFFF,  // 15: White
};

// ── Keyboard ────────────────────────────────────────────────────────────
// Apple II keyboard returns ASCII in bits 0-6, strobe in bit 7
inline constexpr int KEYBOARD_BUFFER_SIZE       = 16;

} // namespace apple_ii_constants
