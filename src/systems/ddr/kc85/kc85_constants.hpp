#pragma once
/*
 * kc85_constants.h — KC 85/2, /3, /4 hardware constants
 *
 * VEB Mikroelektronik Mühlhausen KC 85 series (DDR, 1984–1989)
 *
 * All three share the same base architecture with incremental improvements:
 *   CPU:     U880 (Z80A clone) @ 1.7734475 MHz
 *   I/O:     U855 (Z80 PIO) ×2, U857 (Z80 CTC)
 *   Video:   Custom video controller (320×256, pixel-addressable)
 *            KC85/2,/3: interleaved pixel/color RAM
 *            KC85/4: extended video with two screen planes + independent color
 *   Sound:   Built-in beeper via CTC/PIO; optional sound module
 *   Module:  Expansion slot system for ROM, RAM, and I/O modules
 *
 * KC85/2 (HC 900, 1984): First model, no built-in BASIC, 16 KB RAM
 * KC85/3 (1986): Added built-in BASIC ROM, 16 KB RAM
 * KC85/4 (1989): 64 KB RAM, extended video (two planes), banked memory
 */

#include <cstdint>

namespace kc85_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
inline constexpr uint32_t CPU_FREQ_HZ          = 1773448;   // 1.7734475 MHz (PAL crystal / 2)

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint32_t RAM_SIZE_16K         = 0x4000;    // 16 KB
inline constexpr uint32_t RAM_SIZE_64K         = 0x10000;   // 64 KB (KC85/4)
inline constexpr uint16_t OS_ROM_SIZE          = 0x2000;    // 8 KB CAOS ROM
inline constexpr uint16_t BASIC_ROM_SIZE       = 0x2000;    // 8 KB BASIC ROM

// ── Video ───────────────────────────────────────────────────────────────
inline constexpr int FB_WIDTH                  = 320;
inline constexpr int FB_HEIGHT                 = 256;
inline constexpr int COLOR_COUNT               = 24;      // 16 fg + 8 bg
inline constexpr int FG_COLOR_COUNT             = 16;
inline constexpr int BG_COLOR_OFFSET            = 16;      // bg index = raw_bg + 16
inline constexpr int SCANLINES_PER_FRAME       = 312;       // PAL 312 lines
inline constexpr int KC23_H_TICKS              = 112;       // CPU ticks per scanline (KC85/2,3)
inline constexpr int KC4_H_TICKS               = 113;       // CPU ticks per scanline (KC85/4)

// KC85/2,3: 256×256 pixel-addressable, column-major IRM, 8×4 color cells
// KC85/4:   320×256 pixel-addressable, column-major IRM, per-byte color
inline constexpr int KC23_PIXEL_COLS            = 32;   // 32 bytes × 8 bits = 256 pixels
inline constexpr int KC4_PIXEL_COLS             = 40;   // 40 bytes × 8 bits = 320 pixels
inline constexpr int KC23_COLOR_OFFSET          = 0x2800; // Color RAM starts at IRM + $2800
inline constexpr int KC23_COLOR_CELL_H          = 4;     // Color cell height (pixels)
inline constexpr int KC23_BORDER_X              = 32;    // Left border in 320-px framebuffer

// KC85 palette (ABGR format), 16 foreground + 8 background entries.
// Foreground colors 0-7 are full intensity (0xFF per channel),
// colors 8-15 are mixed hues.  Background colors use 0xA0 for the dim level.
// This matches the floooh/kc85.zig reference palette.
inline constexpr uint32_t PALETTE[24] = {
    // Foreground 0-7 (base colors, full intensity)
    0xFF000000,  //  0: black
    0xFFFF0000,  //  1: blue
    0xFF0000FF,  //  2: red
    0xFFFF00FF,  //  3: magenta
    0xFF00FF00,  //  4: green
    0xFFFFFF00,  //  5: cyan
    0xFF00FFFF,  //  6: yellow
    0xFFFFFFFF,  //  7: white
    // Foreground 8-15 (intensity/hue variants)
    0xFF000000,  //  8: black #2
    0xFFFF00A0,  //  9: violet
    0xFF00A0FF,  // 10: orange
    0xFFA000FF,  // 11: purple
    0xFFA0FF00,  // 12: bluish green
    0xFFFFA000,  // 13: greenish blue
    0xFF00FFA0,  // 14: yellow-green
    0xFFFFFFFF,  // 15: white #2
    // Background 0-7 (dimmed, 0xA0 per channel)
    0xFF000000,  // 16: black
    0xFFA00000,  // 17: dark blue
    0xFF0000A0,  // 18: dark red
    0xFFA000A0,  // 19: dark magenta
    0xFF00A000,  // 20: dark green
    0xFFA0A000,  // 21: dark cyan
    0xFF00A0A0,  // 22: dark yellow
    0xFFA0A0A0,  // 23: gray
};

// ── I/O ports ───────────────────────────────────────────────────────────
inline constexpr uint8_t PIO_A_DATA            = 0x88;
inline constexpr uint8_t CTC_CH0               = 0x8C;
inline constexpr uint8_t KC4_CTRL_PORT         = 0x84;
inline constexpr uint8_t KC4_CTRL2_PORT        = 0x86;
inline constexpr uint8_t MODULE_PORT           = 0x80;
inline constexpr uint8_t MODULE_DATA_PORT      = 0x81;

// ── Keyboard ────────────────────────────────────────────────────────────
inline constexpr int KEYBOARD_ROWS             = 8;

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr uint32_t TSTATES_PER_FRAME    = CPU_FREQ_HZ / 50;  // ≈ 35469
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace kc85_constants
