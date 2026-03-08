#pragma once
/*
 * z1013_constants.h — Robotron Z1013 hardware constants
 *
 * VEB Robotron Z1013 (1985, DDR) — low-cost kit computer.
 *   CPU:     U880 (Z80A clone) @ 2 MHz
 *   I/O:     U855 (Z80 PIO) — keyboard matrix + cassette
 *   Video:   32×32 character display, memory-mapped at $EC00–$EFFF
 *            Character ROM for 8×8 font; BWS (Bildwiederholspeicher) model
 *   Memory:  16 KB RAM ($0000–$3FFF), expandable to 64 KB
 *            2 KB monitor ROM ($F000–$F7FF)
 *   Sound:   None (basic model); optional via expansion
 *   Keyboard: 8×4 membrane matrix (active-low, active-high variants)
 */

#include <cstdint>

namespace z1013_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
inline constexpr uint32_t CPU_FREQ_HZ          = 2000000;   // 2 MHz

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint16_t RAM_BASE             = 0x0000;
inline constexpr uint16_t RAM_SIZE_16K         = 0x4000;    // 16 KB standard
inline constexpr uint32_t RAM_SIZE_64K         = 0x10000;   // 64 KB expanded

inline constexpr uint16_t MONITOR_ROM_BASE     = 0xF000;
inline constexpr uint16_t MONITOR_ROM_SIZE     = 0x0800;    // 2 KB

// Optional 10 KB ROM BASIC ($C000–$E7FF)
inline constexpr uint16_t BASIC_ROM_BASE       = 0xC000;
inline constexpr uint16_t BASIC_ROM_SIZE       = 0x2800;    // 10 KB

// ── Video ───────────────────────────────────────────────────────────────
// Character display: 32×32 = 1024 bytes
inline constexpr uint16_t VIDEO_RAM_BASE       = 0xEC00;
inline constexpr uint16_t VIDEO_RAM_SIZE       = 0x0400;    // 1 KB
inline constexpr int TEXT_COLS                  = 32;
inline constexpr int TEXT_ROWS                  = 32;

// Character ROM: 256 chars × 8 bytes = 2 KB
inline constexpr uint16_t CHAR_ROM_SIZE        = 0x0800;

// Framebuffer for rendering: 256×256 (32×8 × 32×8)
inline constexpr int FB_WIDTH                  = 256;
inline constexpr int FB_HEIGHT                 = 256;

// ── I/O ─────────────────────────────────────────────────────────────────
// PIO at I/O ports $00–$03
inline constexpr uint8_t PIO_PORT_A            = 0x00;      // Keyboard column data
inline constexpr uint8_t PIO_PORT_B            = 0x01;      // Keyboard row select + misc
inline constexpr uint8_t PIO_CTRL_A            = 0x02;
inline constexpr uint8_t PIO_CTRL_B            = 0x03;

// Port $08: keyboard column select (active-low)
inline constexpr uint8_t KEYBOARD_SEL_PORT     = 0x08;

// ── Keyboard ────────────────────────────────────────────────────────────
// 8×4 matrix with active-low scanning
inline constexpr int KEYBOARD_ROWS             = 8;
inline constexpr int KEYBOARD_COLS             = 4;

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr int SCANLINES_PER_FRAME       = 312;       // PAL
inline constexpr int TSTATES_PER_LINE          = 128;       // At 2 MHz
inline constexpr uint32_t TSTATES_PER_FRAME    = SCANLINES_PER_FRAME * TSTATES_PER_LINE;  // 39936
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace z1013_constants
