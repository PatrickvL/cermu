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
inline constexpr int COLOR_COUNT               = 16;

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
