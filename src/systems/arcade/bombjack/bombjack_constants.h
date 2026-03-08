#pragma once
/*
 * bombjack_constants.h — Bomb Jack arcade hardware constants
 *
 * Tehkan Bomb Jack (1984) — dual-Z80 arcade machine
 *   Main CPU:  Z80A @ 4 MHz
 *   Sound CPU: Z80A @ 3 MHz
 *   Sound:     AY-3-8910 ×3 (all on sound CPU bus)
 *   Video:     256×224 pixels, 4-layer rendering:
 *              - Background image (static per stage)
 *              - Scrolling tilemap (32×32 characters, 8×8 tiles)
 *              - Foreground tilemap (32×32, text/score overlay)
 *              - Sprites (24 hardware sprites, 16×16 / 32×32)
 *   Palette:   128 colors from a larger palette bank
 *   Main CPU has 8 KB program ROM + up to 24 KB banked ROM + 2 KB RAM
 *   Sound CPU has 8 KB ROM + shared 1 KB command latch
 */

#include <cstdint>

namespace bombjack_constants {

// ── CPUs ────────────────────────────────────────────────────────────────
inline constexpr uint32_t MAIN_CPU_FREQ_HZ     = 4000000;   // Z80A @ 4 MHz
inline constexpr uint32_t SOUND_CPU_FREQ_HZ    = 3000000;   // Z80A @ 3 MHz

// ── Memory (Main CPU) ───────────────────────────────────────────────────
inline constexpr uint16_t MAIN_ROM_BASE        = 0x0000;
inline constexpr uint16_t MAIN_ROM_SIZE        = 0x8000;    // 32 KB (8 KB × 4 banks)
inline constexpr uint16_t MAIN_RAM_BASE        = 0x8000;
inline constexpr uint16_t MAIN_RAM_SIZE        = 0x1000;    // 4 KB work RAM

// Video RAM
inline constexpr uint16_t FG_TILEMAP_BASE      = 0x9000;    // Foreground tilemap (32×32 = 1 KB)
inline constexpr uint16_t FG_TILEMAP_SIZE      = 0x0400;
inline constexpr uint16_t FG_ATTR_BASE         = 0x9400;    // Foreground attributes (1 KB)
inline constexpr uint16_t BG_TILEMAP_BASE      = 0x9800;    // Background tilemap (not directly here, varies)
inline constexpr uint16_t SPRITE_RAM_BASE      = 0x9820;    // Sprite attribute table
inline constexpr uint16_t SPRITE_RAM_SIZE      = 0x0060;    // 24 sprites × 4 bytes
inline constexpr uint16_t PALETTE_RAM_BASE     = 0x9C00;
inline constexpr uint16_t PALETTE_RAM_SIZE     = 0x0100;    // 256 bytes (128 colors × 2 nibbles)

// I/O
inline constexpr uint16_t INPUT_P1             = 0xB000;    // Player 1 inputs
inline constexpr uint16_t INPUT_P2             = 0xB001;    // Player 2 inputs
inline constexpr uint16_t INPUT_SYSTEM         = 0xB002;    // Coin + start buttons
inline constexpr uint16_t DSW1                 = 0xB003;    // DIP switch bank 1
inline constexpr uint16_t DSW2                 = 0xB004;    // DIP switch bank 2
inline constexpr uint16_t SOUND_LATCH          = 0xB800;    // Write → sound CPU command
inline constexpr uint16_t BG_SELECT            = 0xB004;    // Background image select (write)
inline constexpr uint16_t WATCHDOG             = 0xB800;    // Watchdog reset (write)

// ── Memory (Sound CPU) ──────────────────────────────────────────────────
inline constexpr uint16_t SOUND_ROM_BASE       = 0x0000;
inline constexpr uint16_t SOUND_ROM_SIZE       = 0x2000;    // 8 KB
inline constexpr uint16_t SOUND_RAM_BASE       = 0x4000;
inline constexpr uint16_t SOUND_RAM_SIZE       = 0x0400;    // 1 KB

// AY-3-8910 ports (sound CPU I/O space)
inline constexpr uint8_t AY1_ADDR              = 0x00;
inline constexpr uint8_t AY1_DATA              = 0x01;
inline constexpr uint8_t AY2_ADDR              = 0x10;
inline constexpr uint8_t AY2_DATA              = 0x11;
inline constexpr uint8_t AY3_ADDR              = 0x80;
inline constexpr uint8_t AY3_DATA              = 0x81;
inline constexpr uint8_t SOUND_CMD_READ        = 0x80;      // Sound latch read port (memory-mapped at $6000)

// ── Display ─────────────────────────────────────────────────────────────
inline constexpr int DISPLAY_WIDTH             = 256;
inline constexpr int DISPLAY_HEIGHT            = 224;
inline constexpr int FB_WIDTH                  = 256;
inline constexpr int FB_HEIGHT                 = 224;

inline constexpr int TILE_SIZE                 = 8;
inline constexpr int TILES_X                   = 32;
inline constexpr int TILES_Y                   = 32;

inline constexpr int SPRITE_COUNT              = 24;
inline constexpr int PALETTE_ENTRIES           = 128;
inline constexpr int BG_IMAGE_COUNT            = 5;         // 5 background images

// ── Timing ──────────────────────────────────────────────────────────────
// ~60 Hz refresh (standard Tehkan board)
inline constexpr int REFRESH_HZ               = 60;
inline constexpr uint32_t MAIN_CYCLES_PER_FRAME = MAIN_CPU_FREQ_HZ / REFRESH_HZ;    // ~66667
inline constexpr uint32_t SOUND_CYCLES_PER_FRAME = SOUND_CPU_FREQ_HZ / REFRESH_HZ;  // ~50000
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace bombjack_constants
