#pragma once
/*
 * colecovision_constants.hpp — ColecoVision system constants
 *
 * ColecoVision (1982):
 *   CPU:    Zilog Z80A @ 3.579545 MHz
 *   Video:  TMS9918A VDP (256×192, 15 colors, 32 sprites)
 *   Sound:  SN76489 PSG (3 tone + 1 noise)
 *   Memory: 1KB RAM + 16KB VRAM (in VDP) + 8KB BIOS ROM
 *   Media:  ROM cartridge (8KB-32KB)
 *
 * Memory map:
 *   $0000-$1FFF : 8KB BIOS ROM
 *   $2000-$5FFF : Expansion area
 *   $6000-$7FFF : 1KB RAM (mirrored)
 *   $8000-$FFFF : 32KB Cartridge ROM
 *
 * I/O ports:
 *   $80-$9F : (unused on base unit)
 *   $A0-$BF : VDP (even=data, odd=control)
 *   $C0-$DF : (unused)
 *   $E0-$FF : Controller and SN76489
 *     $E0-$FF even write: controller mode select
 *     $E0-$FF odd read:   controller data
 *     $FF write:          SN76489 data
 */

#include <cstdint>

namespace coleco_constants {

    // ── CPU ─────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ           = 3579545;

    // ── Memory ──────────────────────────────────────────────────────────
    inline constexpr uint32_t BIOS_ROM_SIZE          = 8192;     // 8KB
    inline constexpr uint32_t RAM_SIZE               = 1024;     // 1KB
    inline constexpr uint32_t MAX_CART_SIZE           = 32768;   // 32KB

    // ── I/O ports ───────────────────────────────────────────────────────
    inline constexpr uint8_t  VDP_DATA_PORT          = 0xBE;    // Even addresses $A0-$BE
    inline constexpr uint8_t  VDP_CTRL_PORT          = 0xBF;    // Odd addresses $A1-$BF
    inline constexpr uint8_t  CTRL_MODE_PORT         = 0x80;    // $80 write: controller mode 0
    inline constexpr uint8_t  CTRL_MODE_PORT2        = 0xC0;    // $C0 write: controller mode 1
    inline constexpr uint8_t  CTRL_READ_PORT1        = 0xFC;    // $FC read: controller 1
    inline constexpr uint8_t  CTRL_READ_PORT2        = 0xFF;    // $FF read: controller 2
    inline constexpr uint8_t  PSG_PORT               = 0xFF;    // $FF write: SN76489

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH               = 256;
    inline constexpr int DISPLAY_HEIGHT              = 192;

    // ── Timing ──────────────────────────────────────────────────────────
    inline constexpr uint32_t TSTATES_PER_FRAME_NTSC = 59736;

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE         = 44100;

} // namespace coleco_constants
