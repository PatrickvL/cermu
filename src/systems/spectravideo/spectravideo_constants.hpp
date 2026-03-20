#pragma once
/*
 * spectravideo_constants.hpp — Spectravideo SVI-318 / SVI-328 system constants
 *
 * Spectravideo SVI-318 (1983):
 *   CPU:    Zilog Z80A @ 3.579545 MHz
 *   Video:  TMS9918A VDP (NTSC) / TMS9929A (PAL)
 *   Sound:  AY-3-8910 PSG (3 channels)
 *   I/O:    Intel 8255 PPI (keyboard/cassette/printer)
 *   Memory: 16KB RAM + 32KB ROM (BASIC)
 *   Input:  Chiclet keyboard (rubber keys)
 *
 * Spectravideo SVI-328 (1983):
 *   Same chipset as SVI-318 but with:
 *   - Full-travel keyboard (mechanical)
 *   - 64KB RAM
 *   - Cartridge port compatibility with MSX
 *
 * The SVI series was a direct predecessor to MSX — Spectravideo was
 * instrumental in defining the MSX standard, and the SVI-328 is
 * highly compatible with MSX1 hardware.
 *
 * Memory map:
 *   $0000-$7FFF : 32KB ROM (BASIC interpreter)
 *   $8000-$BFFF : RAM (16KB for SVI-318; shared with cart on SVI-328)
 *   $C000-$FFFF : RAM (SVI-328: upper 16KB; SVI-318: unused/mirror)
 *
 * I/O ports:
 *   $80-$87: VDP (port $80=data, $81=control)
 *   $88-$8B: PSG ($88=addr, $8C=data write, $90=data read)
 *   $96-$97: PPI ($96=port A, $97=port B, etc.)
 */

#include <cstdint>

namespace svi_constants {

    // ── CPU ─────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ           = 3579545;

    // ── Memory ──────────────────────────────────────────────────────────
    inline constexpr uint32_t ROM_SIZE               = 32768;    // 32KB BASIC
    inline constexpr uint32_t RAM_SIZE_SVI318        = 16384;    // 16KB
    inline constexpr uint32_t RAM_SIZE_SVI328        = 65536;    // 64KB

    // ── I/O ports ───────────────────────────────────────────────────────
    inline constexpr uint8_t  VDP_DATA_PORT          = 0x80;
    inline constexpr uint8_t  VDP_CTRL_PORT          = 0x81;
    inline constexpr uint8_t  PSG_ADDR_PORT          = 0x88;
    inline constexpr uint8_t  PSG_DATA_WRITE_PORT    = 0x8C;
    inline constexpr uint8_t  PSG_DATA_READ_PORT     = 0x90;
    inline constexpr uint8_t  PPI_PORT_A             = 0x96;
    inline constexpr uint8_t  PPI_PORT_B             = 0x97;
    inline constexpr uint8_t  PPI_PORT_C             = 0x98;
    inline constexpr uint8_t  PPI_CONTROL            = 0x99;

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH               = 256;
    inline constexpr int DISPLAY_HEIGHT              = 192;

    // ── Timing ──────────────────────────────────────────────────────────
    inline constexpr uint32_t TSTATES_PER_FRAME_NTSC = 59736;
    inline constexpr uint32_t TSTATES_PER_FRAME_PAL  = 71364;

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE         = 44100;

    // ── Keyboard ────────────────────────────────────────────────────────
    inline constexpr int KEYBOARD_ROWS               = 11;
    inline constexpr int KEYBOARD_COLS               = 8;

} // namespace svi_constants
