#pragma once
/*
 * lc80_constants.h — LC 80 learning computer hardware constants
 *
 * VEB Mikroelektronik "Lerncomputer 80" (1984, DDR)
 *   CPU:     U880 (Z80A clone) @ 900 kHz
 *   I/O:     2× U855 (Z80 PIO), 1× U857 (Z80 CTC)
 *   Display: 6× 7-segment LED displays (active-low multiplexed via PIO)
 *   Input:   25-key hex keyboard (active-low matrix via PIO)
 *   Sound:   Piezo speaker driven by CTC channel 2
 *   Memory:  1 KB RAM ($2000–$23FF), expandable to 2 KB
 *            2 KB ROM ($0000–$07FF) — monitor program
 *   No video output — display is entirely LED-based.
 */

#include <cstdint>

namespace lc80_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
inline constexpr uint32_t CPU_FREQ_HZ          = 900000;    // 900 kHz

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint16_t ROM_SIZE             = 0x0800;    // 2 KB monitor ROM
inline constexpr uint16_t RAM_SIZE_MIN         = 0x0400;    // 1 KB standard

// ── I/O ports ───────────────────────────────────────────────────────────
// PIO 1: Port A = LED segment data, Port B = LED digit select + keyboard
inline constexpr uint8_t PIO1_PORT_A           = 0xF4;      // Data port A

// PIO 2: Port A = keyboard scan, Port B = general I/O / cassette
inline constexpr uint8_t PIO2_PORT_A           = 0xF8;

// CTC: 4 channels at $EC–$EF
inline constexpr uint8_t CTC_CH0               = 0xEC;

// ── Display ─────────────────────────────────────────────────────────────
inline constexpr int LED_DIGIT_COUNT           = 6;

// ── Timing ──────────────────────────────────────────────────────────────
// No video frame; we pick an arbitrary update rate for the LED display
inline constexpr int UPDATE_RATE_HZ            = 50;
inline constexpr uint32_t CYCLES_PER_UPDATE    = CPU_FREQ_HZ / UPDATE_RATE_HZ;  // 18000
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace lc80_constants
