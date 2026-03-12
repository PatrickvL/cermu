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

// ── I/O ─────────────────────────────────────────────────────────────────
inline constexpr uint16_t INPUT_P1             = 0xB000;    // Player 1 inputs
inline constexpr uint16_t INPUT_P2             = 0xB001;    // Player 2 inputs
inline constexpr uint16_t INPUT_SYSTEM         = 0xB002;    // Coin + start buttons
inline constexpr uint16_t DSW1                 = 0xB003;    // DIP switch bank 1
inline constexpr uint16_t DSW2                 = 0xB004;    // DIP switch bank 2
inline constexpr uint16_t SOUND_LATCH          = 0xB800;    // Write → sound CPU command
inline constexpr uint16_t BG_SELECT            = 0xB004;    // Background image select (write)

// AY-3-8910 ports (sound CPU I/O space)
inline constexpr uint8_t AY1_ADDR              = 0x00;
inline constexpr uint8_t AY2_ADDR              = 0x10;
inline constexpr uint8_t AY3_ADDR              = 0x80;

// ── Display ─────────────────────────────────────────────────────────────
inline constexpr int FB_WIDTH                  = 256;
inline constexpr int FB_HEIGHT                 = 224;
inline constexpr int PALETTE_ENTRIES           = 128;

// ── Timing ──────────────────────────────────────────────────────────────
// ~60 Hz refresh (standard Tehkan board)
inline constexpr int REFRESH_HZ               = 60;
inline constexpr uint32_t MAIN_CYCLES_PER_FRAME = MAIN_CPU_FREQ_HZ / REFRESH_HZ;    // ~66667
inline constexpr int DEFAULT_SAMPLE_RATE       = 44100;

} // namespace bombjack_constants
