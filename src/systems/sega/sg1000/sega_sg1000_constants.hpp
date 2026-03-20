#pragma once
/*
 * sega_sg1000_constants.hpp — Sega SG-1000 / SC-3000 system constants
 *
 * Sega SG-1000 (1983):
 *   CPU:    Zilog Z80A @ 3.579545 MHz
 *   Video:  TMS9918A VDP (256×192, 15 colors, 32 sprites)
 *   Sound:  SN76489 PSG (3 tone + 1 noise)
 *   Memory: 1KB RAM + 16KB VRAM (in VDP)
 *   Media:  ROM cartridge (8KB-48KB)
 *
 * Sega SC-3000 (1983):
 *   Same chipset as SG-1000, with: full keyboard, BASIC ROM cartridge,
 *   8KB RAM (expandable to 32KB), cassette interface.
 *
 * Memory map:
 *   $0000-$BFFF : Cartridge ROM (up to 48KB)
 *   $C000-$FFFF : 1KB RAM (mirrored through $C000-$FFFF; 8KB for SC-3000)
 *
 * I/O ports:
 *   $7E-$7F : VDP (read: V-counter/H-counter; write: SN76489 data)
 *   $BE-$BF : VDP (read: data/status; write: data/control)
 *   $DC-$DF : I/O controller (joystick ports)
 */

#include <cstdint>

namespace sg1000_constants {

    // ── CPU ─────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ           = 3579545;

    // ── Memory ──────────────────────────────────────────────────────────
    inline constexpr uint32_t RAM_SIZE_SG1000        = 1024;     // 1KB
    inline constexpr uint32_t RAM_SIZE_SC3000        = 8192;     // 8KB (expandable)
    inline constexpr uint32_t MAX_CART_SIZE           = 49152;   // 48KB max cartridge

    // ── VDP I/O ports ───────────────────────────────────────────────────
    inline constexpr uint8_t  VDP_VCOUNTER_PORT      = 0x7E;    // Read: V-counter
    inline constexpr uint8_t  VDP_HCOUNTER_PORT      = 0x7F;    // Read: H-counter
    inline constexpr uint8_t  PSG_PORT               = 0x7E;    // Write: SN76489 data
    inline constexpr uint8_t  VDP_DATA_PORT          = 0xBE;    // Read/Write: VDP data
    inline constexpr uint8_t  VDP_CTRL_PORT          = 0xBF;    // Read: status; Write: control

    // ── I/O controller ──────────────────────────────────────────────────
    inline constexpr uint8_t  IO_PORT_A              = 0xDC;    // Joystick port 1
    inline constexpr uint8_t  IO_PORT_B              = 0xDD;    // Joystick port 2

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH               = 256;
    inline constexpr int DISPLAY_HEIGHT              = 192;

    // ── Timing ──────────────────────────────────────────────────────────
    inline constexpr uint32_t TSTATES_PER_FRAME_NTSC = 59736;   // 262 × 228
    inline constexpr uint32_t TSTATES_PER_FRAME_PAL  = 71364;   // 313 × 228

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE         = 44100;

} // namespace sg1000_constants
