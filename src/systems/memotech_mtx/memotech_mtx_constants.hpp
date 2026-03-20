#pragma once
/*
 * memotech_mtx_constants.hpp — Memotech MTX system constants
 *
 * Memotech MTX500 / MTX512 (1983):
 *   CPU:    Zilog Z80A @ 4 MHz
 *   Video:  TMS9918A VDP (256×192, 15 colors, 32 sprites)
 *   Sound:  General Instrument AY-3-8910 PSG (3 channels)
 *   I/O:    Z80 CTC (baud rate, interrupts), keyboard matrix via Z80 ports
 *   Memory: MTX500: 32KB RAM + 16KB ROM; MTX512: 64KB RAM + 16KB ROM
 *
 * Memory map:
 *   $0000-$1FFF : 8KB OS ROM
 *   $2000-$3FFF : 8KB BASIC ROM (common ROM with OS)
 *   $4000-$7FFF : RAM
 *   $8000-$BFFF : RAM
 *   $C000-$FFFF : RAM (MTX512 only; MTX500 has 32KB total)
 *
 * I/O ports:
 *   $00:     Keyboard sense (active-low column data)
 *   $01-$02: VDP data/control
 *   $05-$06: CTC channels 0-3
 *   $08-$0B: Z80 CTC
 *   $0C-$0F: (unused)
 *   $06:     PSG address latch
 *   $03:     PSG data write
 *   $03:     PSG data read
 */

#include <cstdint>

namespace mtx_constants {

    // ── CPU ─────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ           = 4000000;  // 4 MHz

    // ── Memory ──────────────────────────────────────────────────────────
    inline constexpr uint32_t ROM_SIZE               = 16384;    // 16KB OS+BASIC
    inline constexpr uint32_t RAM_SIZE_MTX500        = 32768;    // 32KB
    inline constexpr uint32_t RAM_SIZE_MTX512        = 65536;    // 64KB

    // ── I/O ports ───────────────────────────────────────────────────────
    inline constexpr uint8_t  KBD_SENSE_PORT         = 0x00;    // Keyboard sense
    inline constexpr uint8_t  VDP_DATA_PORT          = 0x01;    // VDP data
    inline constexpr uint8_t  VDP_CTRL_PORT          = 0x02;    // VDP control
    inline constexpr uint8_t  PSG_DATA_PORT          = 0x03;    // PSG data read/write
    inline constexpr uint8_t  KBD_DRIVE_PORT         = 0x05;    // Keyboard drive (row select)
    inline constexpr uint8_t  PSG_ADDR_PORT          = 0x06;    // PSG address latch
    inline constexpr uint8_t  CTC_BASE_PORT          = 0x08;    // CTC channels 0-3

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH               = 256;
    inline constexpr int DISPLAY_HEIGHT              = 192;

    // ── Timing ──────────────────────────────────────────────────────────
    inline constexpr uint32_t TSTATES_PER_FRAME      = 80000;   // ~50 Hz at 4 MHz

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE         = 44100;

    // ── Keyboard ────────────────────────────────────────────────────────
    inline constexpr int KEYBOARD_ROWS               = 8;
    inline constexpr int KEYBOARD_COLS               = 8;

} // namespace mtx_constants
