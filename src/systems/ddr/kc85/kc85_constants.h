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

// ── Memory — KC85/2 and KC85/3 ──────────────────────────────────────────
inline constexpr uint32_t RAM_SIZE_16K         = 0x4000;    // 16 KB
inline constexpr uint32_t RAM_SIZE_64K         = 0x10000;   // 64 KB (KC85/4)

// ROM
inline constexpr uint16_t OS_ROM_BASE          = 0xE000;    // $E000–$FFFF: 8 KB OS ROM (CAOS)
inline constexpr uint16_t OS_ROM_SIZE          = 0x2000;
inline constexpr uint16_t BASIC_ROM_BASE       = 0xC000;    // $C000–$DFFF: 8 KB BASIC ROM
inline constexpr uint16_t BASIC_ROM_SIZE       = 0x2000;

// ── Video ───────────────────────────────────────────────────────────────
// Display: 320×256 pixels
inline constexpr int DISPLAY_WIDTH             = 320;
inline constexpr int DISPLAY_HEIGHT            = 256;
inline constexpr int FB_WIDTH                  = 320;
inline constexpr int FB_HEIGHT                 = 256;

// Video RAM layout (KC85/2 and /3):
//   Pixel RAM: $8000–$BFFF (16 KB, interleaved)
//   Color RAM: interleaved with pixel RAM (1 byte color per 8×4 block)
inline constexpr uint16_t PIXEL_RAM_BASE       = 0x8000;
inline constexpr uint16_t PIXEL_RAM_SIZE       = 0x4000;    // 16 KB

// KC85/4 extended video: two independent screen planes + separate color banks
// Each plane: 16 KB pixel + 16 KB color, bank-switched
inline constexpr uint16_t KC4_PIXEL_RAM_SIZE   = 0x4000;    // Per plane
inline constexpr uint16_t KC4_COLOR_RAM_SIZE   = 0x4000;    // Per plane

// 16 colors (foreground) × 8 colors (background) + blink + intensity
inline constexpr int COLOR_COUNT               = 16;

// ── I/O ports ───────────────────────────────────────────────────────────
// PIO A / PIO B (accent: accent accent)
inline constexpr uint8_t PIO_A_DATA            = 0x88;      // PIO channel A data
inline constexpr uint8_t PIO_A_CTRL            = 0x8A;
inline constexpr uint8_t PIO_B_DATA            = 0x89;      // PIO channel B data
inline constexpr uint8_t PIO_B_CTRL            = 0x8B;

// CTC: 4 channels at $8C–$8F
inline constexpr uint8_t CTC_CH0               = 0x8C;
inline constexpr uint8_t CTC_CH1               = 0x8D;
inline constexpr uint8_t CTC_CH2               = 0x8E;      // Sound / tape
inline constexpr uint8_t CTC_CH3               = 0x8F;

// System port (PIO B, directly wired):
//   Bit 0: CAOS ROM E ON/OFF
//   Bit 1: RAM bank enable
//   Bit 2: IRM (video RAM) enable
//   Bit 3: RAM bank select (KC85/4)
//   Bit 4: unused
//   Bit 5: LED (active high)
//   Bit 6: BASIC ROM ON/OFF
//   Bit 7: cassette motor

// KC85/4 additional control port
inline constexpr uint8_t KC4_CTRL_PORT         = 0x84;      // Bank switching + video plane select
inline constexpr uint8_t KC4_CTRL2_PORT        = 0x86;      // Extended banking

// Module system I/O
inline constexpr uint8_t MODULE_PORT           = 0x80;      // Module slot control
inline constexpr uint8_t MODULE_DATA_PORT      = 0x81;      // Module data write

// ── Module slots ────────────────────────────────────────────────────────
inline constexpr int MODULE_SLOT_COUNT         = 4;          // 2 internal + 2 via bus connector

// ── Keyboard ────────────────────────────────────────────────────────────
// The KC85 keyboard is directly scanned via PIO port A
inline constexpr int KEYBOARD_COLS             = 8;
inline constexpr int KEYBOARD_ROWS             = 8;

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr int SCANLINES_PER_FRAME       = 312;       // PAL
// Total T-states per frame: CPU_FREQ / 50 ≈ 35469
inline constexpr uint32_t TSTATES_PER_FRAME    = CPU_FREQ_HZ / 50;
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace kc85_constants
