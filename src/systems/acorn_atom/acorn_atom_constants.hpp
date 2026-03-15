#pragma once
/*
 * acorn_atom_constants.h — Acorn Atom hardware constants
 *
 * The Acorn Atom (1980) is a simple 6502-based home computer:
 *   - MOS 6502 CPU @ 1 MHz
 *   - Motorola MC6847 VDG for video
 *   - Intel 8255 PPI (Port A: keyboard row out, Port B: keyboard column in)
 *   - MOS 6522 VIA (optional, usually present for cassette/printer I/O)
 *   - 2KB RAM ($0000–$07FF), expandable to 12KB ($0000–$2FFF)
 *   - 8KB Atom BASIC ROM ($C000–$CFFF, $D000–$DFFF, $E000–$EFFF, $F000–$FFFF)
 *   - 2KB Floating Point ROM ($D000–$D7FF, optional)
 *   - MC6847 video RAM at $8000–$9FFF (6KB for full graphics)
 */

#include <cstdint>

namespace acorn_atom_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
inline constexpr uint32_t CPU_FREQ_HZ         = 1000000;   // 1 MHz

// ── Memory (ROM sizes for loader) ───────────────────────────────────────
inline constexpr uint16_t FP_ROM_SIZE          = 0x0800;    // 2 KB
inline constexpr uint16_t BASIC_ROM_SIZE       = 0x1000;    // 4 KB
inline constexpr uint16_t OS_ROM_SIZE          = 0x1000;    // 4 KB

// ── I/O ─────────────────────────────────────────────────────────────────
inline constexpr uint16_t PPI_BASE             = 0xB000;    // Intel 8255 PPI: $B000–$B003
inline constexpr uint16_t VIA_BASE             = 0xB800;    // MOS 6522 VIA: $B800–$B80F

// ── Display (MC6847 VDG) ────────────────────────────────────────────────
// Text mode: 32×16 characters
inline constexpr int TEXT_COLS                  = 32;
inline constexpr int TEXT_ROWS                  = 16;
// Framebuffer for rendering (MC6847 output)
inline constexpr int FB_WIDTH                  = 256;
inline constexpr int FB_HEIGHT                 = 192;
// MC6847 palette: 8+1 colors (CSS selects green/buff or red/blue set)
inline constexpr int COLOR_COUNT               = 9;

// MC6847 palette for indexed rendering
// Index 0 = black, index 1 = green (phosphor)
inline constexpr uint32_t PALETTE[2] = {
    0xFF000000,  // 0: Black
    0xFF00CC00,  // 1: Green (MC6847 phosphor green)
};

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr int CYCLES_PER_FRAME_PAL      = 312 * 64;  // 19968
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

// ── Keyboard ────────────────────────────────────────────────────────────
// 8255 PPI: Port A = row output (active low strobe), Port B = column input
inline constexpr int KEYBOARD_ROWS             = 10;

} // namespace acorn_atom_constants
