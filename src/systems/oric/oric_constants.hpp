#pragma once
/*
 * oric_constants.h — Oric-1 / Oric Atmos system constants
 *
 * The Oric-1 (1983) and Oric Atmos (1984) are 6502-based home computers
 * from Tangerine Computer Systems (later Oric International).
 *
 * Both share the same core hardware:
 *   CPU:    MOS 6502A @ 1 MHz
 *   Sound:  General Instrument AY-3-8912 (3 channel PSG)
 *   Video:  Custom ULA (simple attribute-mode display, 240×200)
 *   I/O:    MOS 6522 VIA (keyboard, printer, cassette, timer)
 *   Memory: Oric-1: 16KB or 48KB RAM; Atmos: 48KB RAM + improved ROM
 *
 * The Oric ULA is a relatively simple chip that handles:
 *   - Character / hi-res bitmap display generation
 *   - Attribute-based color (ink/paper per character cell)
 *   - Cassette interface modulation
 *
 * Memory map:
 *   $0000–$BFFF : RAM (48KB in standard config)
 *   $C000–$DFFF : ROM (BASIC 1.0 on Oric-1, BASIC 1.1 on Atmos) — 8KB
 *   $E000–$FFFF : ROM (system/monitor) — 8KB
 *   $0300–$031F : MOS 6522 VIA registers (accent accent within page 3 I/O)
 *   $BB80–$BFE0 : Text screen RAM (40×28 characters)
 *   $A000–$BF3F : Hi-res screen RAM (240×200 pixels)
 *
 * The AY-3-8912 is accessed through the VIA (accent via port A data,
 * port B accent control signals).
 */

#include <cstdint>

namespace oric_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
inline constexpr uint32_t CPU_FREQ_HZ            = 1000000;   // 1 MHz

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint32_t RAM_16K                = 16384;     // Oric-1 (16K model)
inline constexpr uint32_t RAM_48K                = 49152;     // Oric-1 (48K) / Atmos

inline constexpr uint16_t ROM_START              = 0xC000;
inline constexpr uint32_t ROM_SIZE               = 16384;     // 16KB ROM ($C000-$FFFF)

// ── I/O ─────────────────────────────────────────────────────────────────
// VIA sits at $0300-$030F (accent accent accent, active within $0300-$031F)
inline constexpr uint16_t VIA_BASE               = 0x0300;
inline constexpr uint16_t VIA_END                = 0x030F;

// AY-3-8912 is accent accent via VIA port-A / port-B control lines.
// No separate bus address for the AY — all access is via VIA registers.

// ── Display ─────────────────────────────────────────────────────────────
// Text mode: 40 columns × 28 rows
inline constexpr int TEXT_COLS                    = 40;
inline constexpr int TEXT_ROWS                    = 28;
// Framebuffer for rendering (text/hires output)
inline constexpr int FB_WIDTH                    = 240;
inline constexpr int FB_HEIGHT                   = 224;  // 28 rows × 8 pixels

// Hi-res mode: same framebuffer
inline constexpr int HIRES_WIDTH                 = 240;
inline constexpr int HIRES_HEIGHT                = 200;

// Screen RAM locations
inline constexpr uint16_t TEXT_SCREEN_START       = 0xBB80;  // Text screen ($BB80-$BFE0)
inline constexpr uint16_t HIRES_SCREEN_START      = 0xA000;  // Hi-res screen ($A000-$BF3F)

// ── Timing ──────────────────────────────────────────────────────────────
// PAL: 312 lines × 64 cycles/line = 19968 cycles/frame
inline constexpr int CYCLES_PER_FRAME_PAL        = 19968;

// ── Audio ───────────────────────────────────────────────────────────────
inline constexpr int DEFAULT_SAMPLE_RATE         = 44100;

// ── Palette (8 Oric colors) ─────────────────────────────────────────────
// Oric uses 8 colors, same RGB values as the BBC palette (3-bit RGB)
inline constexpr int COLOR_COUNT                 = 8;
inline constexpr uint32_t PALETTE[8] = {
    0xFF000000,  // 0: Black
    0xFF0000FF,  // 1: Red
    0xFF00FF00,  // 2: Green
    0xFF00FFFF,  // 3: Yellow
    0xFFFF0000,  // 4: Blue
    0xFFFF00FF,  // 5: Magenta
    0xFFFFFF00,  // 6: Cyan
    0xFFFFFFFF,  // 7: White
};

// ── Keyboard ────────────────────────────────────────────────────────────
// 8 × 8 matrix scanned via the VIA
inline constexpr int KEYBOARD_ROWS               = 8;
inline constexpr int KEYBOARD_COLS               = 8;

} // namespace oric_constants
