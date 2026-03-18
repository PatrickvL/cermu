#pragma once
/*
 * c128_constants.h — Commodore 128 system constants
 *
 * The Commodore 128 (1985) was Commodore's successor to the C64,
 * featuring three operating modes:
 *   - C128 mode (native, with 128KB RAM and enhanced BASIC 7.0)
 *   - C64 mode (full hardware compatibility with the C64)
 *   - CP/M mode (Z80 CPU running CP/M 3.0)
 *
 * Hardware:
 *   CPU:   CSG 8502 (6502-compatible) @ 1/2 MHz + Zilog Z80 @ 4 MHz
 *   Video: MOS 8564/8566 VIC-IIe (enhanced VIC-II) + MOS 8563 VDC (80-col)
 *   Sound: MOS 6581/8580 SID
 *   I/O:   2× MOS 6526 CIA
 *   MMU:   MOS 8722 (bank switching, mode control)
 *   RAM:   128KB (expandable to 640KB via REU)
 *   ROM:   64KB (BASIC 7.0, KERNAL, character ROM, editor)
 */

#include <cstdint>

namespace c128_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
inline constexpr uint32_t CPU_FREQ_1MHZ_PAL     = 985248;    // 8502 slow mode (PAL)
inline constexpr uint32_t CPU_FREQ_2MHZ_PAL     = 1970496;   // 8502 fast mode (PAL)
inline constexpr uint32_t CPU_FREQ_1MHZ_NTSC    = 1022727;   // 8502 slow mode (NTSC)
inline constexpr uint32_t CPU_FREQ_2MHZ_NTSC    = 2045454;   // 8502 fast mode (NTSC)
inline constexpr uint32_t Z80_FREQ              = 4000000;   // Z80 @ 4 MHz

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint32_t RAM_SIZE              = 131072;    // 128 KB
inline constexpr uint32_t ROM_SIZE              = 65536;     // 64 KB total ROM

// ROM layout (C128 mode)
inline constexpr uint16_t BASIC_LO_START        = 0x4000;    // BASIC 7.0 low ($4000-$7FFF)
inline constexpr uint16_t BASIC_HI_START        = 0x8000;    // BASIC 7.0 high ($8000-$BFFF)
inline constexpr uint16_t EDITOR_START          = 0xC000;    // Editor ROM ($C000-$CFFF)
inline constexpr uint16_t KERNAL_START          = 0xE000;    // KERNAL ($E000-$FFFF)
inline constexpr uint16_t CHARGEN_SIZE          = 4096;      // Character generator ROM

// ── I/O ─────────────────────────────────────────────────────────────────
inline constexpr uint16_t IO_START              = 0xD000;
inline constexpr uint16_t IO_END                = 0xDFFF;

inline constexpr uint16_t VIC_BASE              = 0xD000;    // VIC-IIe: $D000-$D3FF
inline constexpr uint16_t SID_BASE              = 0xD400;    // SID: $D400-$D7FF
inline constexpr uint16_t MMU_BASE              = 0xD500;    // 8722 MMU: $D500-$D50B
inline constexpr uint16_t VDC_BASE              = 0xD600;    // 8563 VDC: $D600-$D601
inline constexpr uint16_t CIA1_BASE             = 0xDC00;    // CIA 1: $DC00-$DCFF
inline constexpr uint16_t CIA2_BASE             = 0xDD00;    // CIA 2: $DD00-$DDFF

// ── 8722 MMU registers ─────────────────────────────────────────────────
// The 8722 MMU controls all bank switching and mode selection.
// It has 12 registers at $D500-$D50B plus additional registers
// at $FF00-$FF04 (mirrored configuration registers).
inline constexpr uint16_t MMU_CR                = 0xD500;    // Configuration register
inline constexpr uint16_t MMU_PCR_A             = 0xD501;    // Preconfiguration reg A
inline constexpr uint16_t MMU_PCR_B             = 0xD502;    // Preconfiguration reg B
inline constexpr uint16_t MMU_PCR_C             = 0xD503;    // Preconfiguration reg C
inline constexpr uint16_t MMU_PCR_D             = 0xD504;    // Preconfiguration reg D
inline constexpr uint16_t MMU_MCR               = 0xD505;    // Mode config register
inline constexpr uint16_t MMU_RCR               = 0xD506;    // RAM config register
inline constexpr uint16_t MMU_P0L               = 0xD507;    // Page 0 pointer low
inline constexpr uint16_t MMU_P0H               = 0xD508;    // Page 0 pointer high
inline constexpr uint16_t MMU_P1L               = 0xD509;    // Page 1 pointer low
inline constexpr uint16_t MMU_P1H               = 0xD50A;    // Page 1 pointer high
inline constexpr uint16_t MMU_VERSION           = 0xD50B;    // Version register (read-only)

// ── Display (VIC-IIe — 40-col) ─────────────────────────────────────────
inline constexpr int VIC_DISPLAY_WIDTH_PAL      = 403;
inline constexpr int VIC_DISPLAY_HEIGHT_PAL     = 284;
inline constexpr int VIC_DISPLAY_WIDTH_NTSC     = 418;
inline constexpr int VIC_DISPLAY_HEIGHT_NTSC    = 235;

// ── Display (8563 VDC — 80-col) ────────────────────────────────────────
inline constexpr int VDC_DISPLAY_WIDTH          = 640;       // 80 columns × 8 pixels
inline constexpr int VDC_DISPLAY_HEIGHT         = 200;       // 25 rows × 8 pixels

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr uint32_t CYCLES_PER_FRAME_PAL  = 19656;    // Same as C64 PAL
inline constexpr uint32_t CYCLES_PER_FRAME_NTSC = 17095;    // Same as C64 NTSC

// ── Audio ───────────────────────────────────────────────────────────────
inline constexpr int DEFAULT_SAMPLE_RATE        = 44100;

// ── Keyboard ────────────────────────────────────────────────────────────
// C128 keyboard matrix: 11 columns × 8 rows (C64 matrix + extra keys)
// Active-low scanning via CIA 1 ports A and B
inline constexpr int KEYBOARD_COLS              = 11;
inline constexpr int KEYBOARD_ROWS              = 8;

} // namespace c128_constants
