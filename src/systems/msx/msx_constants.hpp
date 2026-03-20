#pragma once
/*
 * msx_constants.hpp — MSX family system constants
 *
 * MSX1 (1983):
 *   CPU:    Zilog Z80A @ 3.579545 MHz
 *   Video:  TMS9918A (NTSC) / TMS9928A (RGB) / TMS9929A (PAL)
 *   Sound:  AY-3-8910 PSG (3 channels + noise + envelope)
 *   I/O:    Intel 8255 PPI (keyboard matrix, cassette, slot control)
 *   Memory: 32KB ROM (BIOS + BASIC) + 8/16/32/64KB RAM
 *
 * MSX2 (1985):
 *   CPU:    Zilog Z80A @ 3.579545 MHz
 *   Video:  Yamaha V9938 (128KB VRAM, bitmap modes, blitter)
 *   Sound:  AY-3-8910 PSG
 *   I/O:    Intel 8255 PPI + MSX-Engine extensions
 *   Memory: 32KB ROM + 64/128KB RAM, memory mapper
 *
 * MSX2+ (1988):
 *   CPU:    Zilog Z80A @ 3.579545 MHz
 *   Video:  Yamaha V9958 (YJK modes, horizontal scroll)
 *   Sound:  AY-3-8910 PSG
 *   I/O:    Intel 8255 PPI
 *   Memory: 32KB ROM + 64KB RAM minimum
 *
 * Memory map (primary slot layout):
 *   $0000-$3FFF: Slot 0 — BIOS ROM (32KB shared with $4000)
 *   $4000-$7FFF: Slot 0 — BASIC ROM
 *   $8000-$BFFF: Slot 3 — RAM (or cartridge)
 *   $C000-$FFFF: Slot 3 — RAM
 *
 * I/O ports:
 *   $98-$9B:  VDP (TMS9918A / V9938 / V9958)
 *   $A0-$A3:  PSG (AY-3-8910)
 *   $A8-$AB:  PPI (i8255)
 */

#include <cstdint>

namespace msx_constants {

    // ── CPU ─────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ             = 3579545;  // NTSC color burst

    // ── Memory ──────────────────────────────────────────────────────────
    inline constexpr uint32_t BIOS_ROM_SIZE           = 32768;    // 32KB BIOS+BASIC
    inline constexpr uint32_t RAM_SIZE_MSX1           = 65536;    // 64KB (common MSX1)
    inline constexpr uint32_t RAM_SIZE_MSX2           = 131072;   // 128KB (common MSX2)
    inline constexpr uint32_t RAM_SIZE_MSX2P          = 65536;    // 64KB minimum MSX2+

    // ── VDP I/O ports (directly addressed by Z80 port I/O) ─────────────
    inline constexpr uint8_t  VDP_DATA_PORT           = 0x98;
    inline constexpr uint8_t  VDP_CTRL_PORT           = 0x99;
    inline constexpr uint8_t  VDP_PALETTE_PORT        = 0x9A;   // V9938+ only
    inline constexpr uint8_t  VDP_INDIRECT_PORT       = 0x9B;   // V9938+ only

    // ── PSG I/O ports ───────────────────────────────────────────────────
    inline constexpr uint8_t  PSG_ADDR_PORT           = 0xA0;   // Write: latch register
    inline constexpr uint8_t  PSG_DATA_WRITE_PORT     = 0xA1;   // Write: register data
    inline constexpr uint8_t  PSG_DATA_READ_PORT      = 0xA2;   // Read: register data

    // ── PPI I/O ports ───────────────────────────────────────────────────
    inline constexpr uint8_t  PPI_PORT_A              = 0xA8;   // Primary slot select
    inline constexpr uint8_t  PPI_PORT_B              = 0xA9;   // Keyboard column read
    inline constexpr uint8_t  PPI_PORT_C              = 0xAA;   // Keyboard row select + cassette + caps LED
    inline constexpr uint8_t  PPI_CONTROL             = 0xAB;   // PPI control word

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH_MSX1           = 256;
    inline constexpr int DISPLAY_HEIGHT_MSX1          = 192;
    inline constexpr int DISPLAY_WIDTH_MSX2           = 512;    // Max for V9938 modes
    inline constexpr int DISPLAY_HEIGHT_MSX2          = 212;    // V9938 extended

    // ── Timing ──────────────────────────────────────────────────────────
    inline constexpr uint32_t TSTATES_PER_FRAME_NTSC  = 59736;  // 262 × 228
    inline constexpr uint32_t TSTATES_PER_FRAME_PAL   = 71364;  // 313 × 228

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE          = 44100;

    // ── Keyboard ────────────────────────────────────────────────────────
    inline constexpr int KEYBOARD_ROWS                = 11;
    inline constexpr int KEYBOARD_COLS                = 8;

} // namespace msx_constants
