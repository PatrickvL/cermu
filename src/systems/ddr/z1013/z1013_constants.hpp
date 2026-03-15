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
inline constexpr uint16_t RAM_SIZE_16K         = 0x4000;    // 16 KB standard
inline constexpr uint32_t RAM_SIZE_64K         = 0x10000;   // 64 KB expanded
inline constexpr uint16_t MONITOR_ROM_SIZE     = 0x0800;    // 2 KB
inline constexpr uint16_t BASIC_ROM_SIZE       = 0x2800;    // 10 KB ROM BASIC

// ── Video ───────────────────────────────────────────────────────────────
inline constexpr int TEXT_COLS                  = 32;
inline constexpr int TEXT_ROWS                  = 32;
inline constexpr uint16_t CHAR_ROM_SIZE        = 0x0800;    // 256 chars × 8 bytes = 2 KB
inline constexpr int FB_WIDTH                  = 256;       // 32×8 × 32×8
inline constexpr int FB_HEIGHT                 = 256;

// Monochrome palette: index 0 = background (black), index 1 = foreground (white)
inline constexpr uint32_t PALETTE[2] = {
    0xFF000000,  // 0: Black
    0xFFFFFFFF,  // 1: White
};

// ── I/O ─────────────────────────────────────────────────────────────────
inline constexpr uint8_t PIO_PORT_A            = 0x00;      // Keyboard column data
inline constexpr uint8_t PIO_CTRL_B            = 0x03;
inline constexpr uint8_t KEYBOARD_SEL_PORT     = 0x08;      // Keyboard column select

// ── Keyboard ────────────────────────────────────────────────────────────
inline constexpr int KEYBOARD_ROWS             = 8;

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr uint32_t TSTATES_PER_FRAME    = 312 * 128; // 39936 (312 PAL lines × 128 T-states)
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace z1013_constants
