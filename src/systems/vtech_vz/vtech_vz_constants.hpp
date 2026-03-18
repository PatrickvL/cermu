#pragma once
/*
 * vtech_vz_constants.h — VTech VZ200 / VZ300 system constants
 *
 * Also known as:
 *   VZ200:  Dick Smith VZ200, Laser 200, Salora Fellow
 *   VZ300:  Dick Smith VZ300, Laser 310, Salora Fellow II
 *
 * Both are Z80-based home computers with MC6847 video display:
 *   CPU:    Zilog Z80A @ 3.579545 MHz (NTSC color burst)
 *   Video:  Motorola MC6847 VDG (text 32×16, graphics 256×192/128×64)
 *   Sound:  Speaker driven by Z80 port (1-bit) — no dedicated sound chip
 *   I/O:    Memory-mapped I/O + Z80 IN/OUT ports
 *   Memory: VZ200: 8KB RAM + 16KB ROM; VZ300: 16KB RAM + 16KB ROM
 *
 * Memory map:
 *   $0000–$3FFF : 16KB ROM (BASIC interpreter)
 *   $4000–$67FF : Unused / expansion
 *   $6800–$6FFF : I/O region (keyboard, display mode, cassette, speaker)
 *   $7000–$77FF : Video RAM (2KB — MC6847 display data)
 *   $7800–$7FFF : VZ200: RAM start / VZ300: additional video attributes
 *   $7800–$B7FF : VZ200: 8KB user RAM ($7800–$97FF)
 *   $7800–$B7FF : VZ300: 16KB user RAM ($7800–$B7FF)
 *   $B800–$FFFF : Expansion area
 */

#include <cstdint>

namespace vtech_vz_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
// Clock derived from NTSC color burst: 3.579545 MHz
inline constexpr uint32_t CPU_FREQ_HZ           = 3579545;

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint16_t ROM_START             = 0x0000;
inline constexpr uint32_t ROM_SIZE              = 16384;    // 16 KB BASIC ROM
inline constexpr uint16_t VIDEO_RAM_START       = 0x7000;
inline constexpr uint32_t VIDEO_RAM_SIZE        = 2048;     // 2 KB
inline constexpr uint16_t USER_RAM_START        = 0x7800;

inline constexpr uint32_t RAM_SIZE_VZ200        = 8192;     // 8 KB user RAM
inline constexpr uint32_t RAM_SIZE_VZ300        = 16384;    // 16 KB user RAM

// ── I/O ─────────────────────────────────────────────────────────────────
// I/O is memory-mapped at $6800-$6FFF:
//   $6800: Keyboard latch (active-low, accent)
//   $6800: Display mode / cassette / speaker out (accent accent)
inline constexpr uint16_t IO_BASE               = 0x6800;
inline constexpr uint16_t IO_END                = 0x6FFF;

// Z80 port I/O:
//   Port $00-$0F: unused
//   Port $20-$2F: Speaker bit (accent)
inline constexpr uint8_t  PORT_SPEAKER          = 0x20;

// ── Display (MC6847 VDG) ────────────────────────────────────────────────
// Text mode: 32 columns × 16 rows (like Acorn Atom)
inline constexpr int TEXT_COLS                   = 32;
inline constexpr int TEXT_ROWS                   = 16;
// Framebuffer for rendering (MC6847 output)
inline constexpr int FB_WIDTH                    = 256;
inline constexpr int FB_HEIGHT                   = 192;
// MC6847 palette: 9 colors (same as Acorn Atom)
inline constexpr int COLOR_COUNT                 = 9;

// ── Timing ──────────────────────────────────────────────────────────────
// PAL: 312 lines × 228 T-states/line ≈ 71136 T-states/frame
// NTSC: 262 lines × 228 T-states/line ≈ 59736 T-states/frame
inline constexpr int TSTATES_PER_FRAME_PAL       = 71136;
inline constexpr int TSTATES_PER_FRAME_NTSC      = 59736;

// ── Audio ───────────────────────────────────────────────────────────────
inline constexpr int DEFAULT_SAMPLE_RATE         = 44100;

// ── Keyboard ────────────────────────────────────────────────────────────
// 8 × 6 matrix (accent-addressed via I/O)
inline constexpr int KEYBOARD_ROWS               = 8;
inline constexpr int KEYBOARD_COLS               = 6;

} // namespace vtech_vz_constants
