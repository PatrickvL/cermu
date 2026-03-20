#pragma once
/*
 * tatung_einstein_constants.hpp — Tatung Einstein system constants
 *
 * Tatung Einstein TC-01 (1984):
 *   CPU:    Zilog Z80A @ 4 MHz
 *   Video:  TMS9929A VDP (PAL, 256×192, 15 colors, 32 sprites)
 *   Sound:  AY-3-8910 PSG (3 channels + noise + envelope, I/O ports used for keyboard)
 *   I/O:    Z80 CTC (4 channels), Z80 PIO (parallel I/O), i8251 USART
 *   Memory: 64KB RAM + 8KB ROM (OS)
 *
 * The Einstein was a CP/M-compatible Z80 machine from Tatung (Taiwan),
 * notable for its use of PAL TMS9929A video and built-in disk drive.
 *
 * Memory map:
 *   $0000-$1FFF : 8KB ROM (can be banked out for full 64KB RAM)
 *   $0000-$FFFF : 64KB RAM (ROM overlays first 8KB at boot)
 *
 * I/O ports:
 *   $00:     PSG address latch
 *   $01:     PSG data write
 *   $02:     PSG data read
 *   $03:     VDP data
 *   $04:     VDP control/status
 *   $08-$0B: Z80 CTC channels
 *   $10-$13: Z80 PIO (port A/B data/control)
 *   $20:     Keyboard data (directly via PSG I/O port A)
 *   $21:     Keyboard drive (via PSG I/O port B)
 *   $23:     ROM bank control (write $00 to disable ROM overlay)
 */

#include <cstdint>

namespace einstein_constants {

    // ── CPU ─────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ           = 4000000;  // 4 MHz

    // ── Memory ──────────────────────────────────────────────────────────
    inline constexpr uint32_t ROM_SIZE               = 8192;     // 8KB OS ROM
    inline constexpr uint32_t RAM_SIZE               = 65536;    // 64KB

    // ── I/O ports ───────────────────────────────────────────────────────
    inline constexpr uint8_t  PSG_ADDR_PORT          = 0x00;
    inline constexpr uint8_t  PSG_DATA_WRITE_PORT    = 0x01;
    inline constexpr uint8_t  PSG_DATA_READ_PORT     = 0x02;
    inline constexpr uint8_t  VDP_DATA_PORT          = 0x03;
    inline constexpr uint8_t  VDP_CTRL_PORT          = 0x04;
    inline constexpr uint8_t  CTC_BASE_PORT          = 0x08;
    inline constexpr uint8_t  PIO_BASE_PORT          = 0x10;
    inline constexpr uint8_t  ROM_BANK_PORT          = 0x23;

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH               = 256;
    inline constexpr int DISPLAY_HEIGHT              = 192;

    // ── Timing ──────────────────────────────────────────────────────────
    // PAL: 313 lines × ~256 clocks/line ≈ 80128 T-states/frame @ 50 Hz
    inline constexpr uint32_t TSTATES_PER_FRAME      = 80000;

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE         = 44100;

    // ── Keyboard ────────────────────────────────────────────────────────
    inline constexpr int KEYBOARD_ROWS               = 8;
    inline constexpr int KEYBOARD_COLS               = 8;

} // namespace einstein_constants
