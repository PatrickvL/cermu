#pragma once
/*
 * sega_sms_constants.hpp — Sega Master System constants
 *
 * Sega Master System (1985 Japan / 1986 worldwide):
 *   CPU:    Zilog Z80A @ 3.579545 MHz (NTSC) / 3.546895 MHz (PAL)
 *   Video:  Sega 315-5124 VDP (TMS9918A-compatible + Mode 4)
 *   Sound:  SN76489 PSG (integrated in VDP)
 *   Memory: 8KB RAM + 16KB VRAM (in VDP)
 *   Media:  ROM cartridge (up to 512KB with banking)
 *
 * Memory map:
 *   $0000-$03FF : ROM bank 0 (fixed first 1KB)
 *   $0400-$3FFF : ROM bank 0 (pages via $FFFD)
 *   $4000-$7FFF : ROM bank 1 (pages via $FFFE)
 *   $8000-$BFFF : ROM bank 2 (pages via $FFFF) or Cartridge RAM
 *   $C000-$DFFF : 8KB System RAM
 *   $E000-$FFFF : System RAM mirror
 *
 * I/O ports:
 *   $3E:     Memory control (enable/disable BIOS, cartridge, expansion)
 *   $3F:     I/O port control
 *   $7E-$7F: VDP (read: V/H counter; write: SN76489)
 *   $BE-$BF: VDP (read/write: data/control)
 *   $DC-$DD: I/O ports (joypads)
 */

#include <cstdint>

namespace sms_constants {

    // ── CPU ─────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ_NTSC       = 3579545;
    inline constexpr uint32_t CPU_FREQ_HZ_PAL        = 3546895;

    // ── Memory ──────────────────────────────────────────────────────────
    inline constexpr uint32_t SYSTEM_RAM_SIZE         = 8192;     // 8KB
    inline constexpr uint32_t MAX_CART_SIZE            = 524288;   // 512KB max
    inline constexpr uint32_t BIOS_ROM_SIZE            = 8192;     // 8KB (if present)

    // ── I/O ports ───────────────────────────────────────────────────────
    inline constexpr uint8_t  MEM_CTRL_PORT            = 0x3E;
    inline constexpr uint8_t  IO_CTRL_PORT             = 0x3F;
    inline constexpr uint8_t  VCOUNTER_PORT            = 0x7E;
    inline constexpr uint8_t  HCOUNTER_PORT            = 0x7F;
    inline constexpr uint8_t  PSG_PORT                 = 0x7E;    // Write only
    inline constexpr uint8_t  VDP_DATA_PORT            = 0xBE;
    inline constexpr uint8_t  VDP_CTRL_PORT            = 0xBF;
    inline constexpr uint8_t  IO_PORT_A                = 0xDC;
    inline constexpr uint8_t  IO_PORT_B                = 0xDD;

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH                 = 256;
    inline constexpr int DISPLAY_HEIGHT                = 192;

    // ── Timing ──────────────────────────────────────────────────────────
    inline constexpr uint32_t TSTATES_PER_FRAME_NTSC   = 59736;
    inline constexpr uint32_t TSTATES_PER_FRAME_PAL    = 71364;

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE           = 44100;

    // ── Mapper registers (at top of RAM) ────────────────────────────────
    inline constexpr uint16_t MAPPER_CTRL              = 0xFFFC;
    inline constexpr uint16_t MAPPER_BANK0             = 0xFFFD;
    inline constexpr uint16_t MAPPER_BANK1             = 0xFFFE;
    inline constexpr uint16_t MAPPER_BANK2             = 0xFFFF;

} // namespace sms_constants
