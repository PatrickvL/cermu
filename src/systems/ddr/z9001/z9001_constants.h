#pragma once
/*
 * z9001_constants.h — Robotron Z9001 / KC 87 hardware constants
 *
 * VEB Robotron Z9001 (1984) / KC 87 (1987, DDR)
 *   CPU:     U880 (Z80A clone) @ 2.4576 MHz
 *   I/O:     2× U855 (Z80 PIO), 1× U857 (Z80 CTC)
 *   Video:   MC6845-style text display, 40×24 characters (via custom circuitry)
 *   Memory:  Z9001: 16 KB RAM; KC 87: 48 KB RAM
 *            4 KB OS ROM + optional 10 KB BASIC ROM
 *   Sound:   Beeper (via PIO or CTC)
 *   Keyboard: Full-stroke keyboard matrix (active-low)
 *
 * KC 87 is the enhanced version with more RAM + built-in BASIC + color attribute RAM.
 */

#include <cstdint>

namespace z9001_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
inline constexpr uint32_t CPU_FREQ_HZ          = 2457600;   // 2.4576 MHz

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint16_t RAM_BASE             = 0x0000;
inline constexpr uint32_t RAM_SIZE_Z9001       = 0x4000;    // 16 KB
inline constexpr uint32_t RAM_SIZE_KC87        = 0xC000;    // 48 KB

inline constexpr uint16_t OS_ROM_BASE          = 0xF000;    // $F000–$FFFF
inline constexpr uint16_t OS_ROM_SIZE          = 0x1000;    // 4 KB

inline constexpr uint16_t BASIC_ROM_BASE       = 0xC000;    // $C000–$E7FF
inline constexpr uint16_t BASIC_ROM_SIZE       = 0x2800;    // 10 KB

// ── Video ───────────────────────────────────────────────────────────────
// Text display: 40×24 characters
inline constexpr uint16_t VIDEO_RAM_BASE       = 0xEC00;    // 40×24 = 960 bytes
inline constexpr uint16_t VIDEO_RAM_SIZE       = 0x0400;    // 1 KB
inline constexpr uint16_t COLOR_RAM_BASE       = 0xE800;    // KC 87 only
inline constexpr uint16_t COLOR_RAM_SIZE       = 0x0400;    // 1 KB
inline constexpr int TEXT_COLS                  = 40;
inline constexpr int TEXT_ROWS                  = 24;

// Character ROM: 256 chars × 8 bytes = 2 KB
inline constexpr uint16_t CHAR_ROM_SIZE        = 0x0800;

// Framebuffer: 320×192 (40×8 × 24×8)
inline constexpr int FB_WIDTH                  = 320;
inline constexpr int FB_HEIGHT                 = 192;

// 8 foreground colors + 8 background colors (KC 87 with color RAM)
inline constexpr int COLOR_COUNT               = 8;

// ── I/O ─────────────────────────────────────────────────────────────────
// PIO 1: Port A = keyboard row, Port B = system control / color
inline constexpr uint8_t PIO1_PORT_A           = 0x88;
inline constexpr uint8_t PIO1_PORT_B           = 0x89;
inline constexpr uint8_t PIO1_CTRL_A           = 0x8A;
inline constexpr uint8_t PIO1_CTRL_B           = 0x8B;

// PIO 2: Port A = keyboard, Port B = cassette / joystick
inline constexpr uint8_t PIO2_PORT_A           = 0x90;
inline constexpr uint8_t PIO2_PORT_B           = 0x91;
inline constexpr uint8_t PIO2_CTRL_A           = 0x92;
inline constexpr uint8_t PIO2_CTRL_B           = 0x93;

// CTC: 4 channels
inline constexpr uint8_t CTC_CH0               = 0x80;
inline constexpr uint8_t CTC_CH1               = 0x81;
inline constexpr uint8_t CTC_CH2               = 0x82;      // Sound
inline constexpr uint8_t CTC_CH3               = 0x83;

// ── Keyboard ────────────────────────────────────────────────────────────
inline constexpr int KEYBOARD_ROWS             = 8;
inline constexpr int KEYBOARD_COLS             = 8;

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr int SCANLINES_PER_FRAME       = 312;       // PAL
inline constexpr int TSTATES_PER_LINE          = 128;       // ~128 at 2.4576 MHz
inline constexpr uint32_t TSTATES_PER_FRAME    = 49152;     // ~312 × 157.5 (exact TBD)
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace z9001_constants
