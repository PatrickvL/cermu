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
inline constexpr uint32_t RAM_SIZE_Z9001       = 0x4000;    // 16 KB
inline constexpr uint32_t RAM_SIZE_KC87        = 0xC000;    // 48 KB
inline constexpr uint16_t OS_ROM_SIZE          = 0x1000;    // 4 KB
inline constexpr uint16_t BASIC_ROM_SIZE       = 0x2800;    // 10 KB

// ── Video ───────────────────────────────────────────────────────────────
inline constexpr int TEXT_COLS                  = 40;
inline constexpr int TEXT_ROWS                  = 24;
inline constexpr uint16_t CHAR_ROM_SIZE        = 0x0800;    // 256 chars × 8 bytes = 2 KB
inline constexpr int FB_WIDTH                  = 320;       // 40×8 × 24×8
inline constexpr int FB_HEIGHT                 = 192;
inline constexpr int COLOR_COUNT               = 8;

// ── I/O ─────────────────────────────────────────────────────────────────
inline constexpr uint8_t PIO1_PORT_A           = 0x88;
inline constexpr uint8_t PIO1_CTRL_B           = 0x8B;
inline constexpr uint8_t PIO2_PORT_A           = 0x90;
inline constexpr uint8_t PIO2_CTRL_B           = 0x93;
inline constexpr uint8_t CTC_CH0               = 0x80;
inline constexpr uint8_t CTC_CH3               = 0x83;

// ── Keyboard ────────────────────────────────────────────────────────────
inline constexpr int KEYBOARD_ROWS             = 8;

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr uint32_t TSTATES_PER_FRAME    = 49152;
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace z9001_constants
