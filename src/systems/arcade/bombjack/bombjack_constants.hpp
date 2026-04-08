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
#include "core/dip_switch.hpp"

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
inline constexpr uint16_t BG_SELECT            = 0x9E00;    // Background image select (write)
inline constexpr uint16_t NMI_MASK             = 0xB000;    // Write: NMI enable mask
inline constexpr uint16_t FLIP_SCREEN          = 0xB004;    // Write: flip screen

// ── Sprites ─────────────────────────────────────────────────────────────
inline constexpr uint16_t SPRITE_RAM_BASE      = 0x9820;    // First sprite attribute
inline constexpr int      SPRITE_COUNT         = 24;        // Hardware sprites
inline constexpr int      SPRITE_BYTES         = 4;         // Bytes per sprite

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

// ── DIP switches ────────────────────────────────────────────────────────
//
// DSW1 ($B003 read) — coinage and cabinet type
// DSW2 ($B004 read) — lives, bonus, difficulty, demo sounds
//
// Bit patterns match Tehkan Bomb Jack PCB manual.  All active-low:
// factory default = 0xFF (all switches OFF).

// ── DSW1 settings ───────────────────────────────────────────────────────

inline constexpr DipSetting kBjCoinA[] = {
    { "1 Coin / 1 Credit",   0x00 },   // default (bit pattern in mask)
    { "1 Coin / 2 Credits",  0x01 },
    { "1 Coin / 3 Credits",  0x02 },
    { "1 Coin / 6 Credits",  0x03 },
    { "2 Coins / 1 Credit",  0x04 },
    { "3 Coins / 1 Credit",  0x05 },
    { "4 Coins / 1 Credit",  0x06 },
    { "5 Coins / 1 Credit",  0x07 },
};

inline constexpr DipSetting kBjCoinB[] = {
    { "1 Coin / 1 Credit",   0x00 },   // default
    { "1 Coin / 2 Credits",  0x08 },
    { "1 Coin / 3 Credits",  0x10 },
    { "1 Coin / 6 Credits",  0x18 },
    { "2 Coins / 1 Credit",  0x20 },
    { "3 Coins / 1 Credit",  0x28 },
    { "4 Coins / 1 Credit",  0x30 },
    { "5 Coins / 1 Credit",  0x38 },
};

inline constexpr DipSetting kBjCabinet[] = {
    { "Upright",   0x00 },   // default
    { "Cocktail",  0x80 },
};

inline constexpr DipSwitch kBjDsw1Switches[] = {
    { "Coin A",    0x07, 0, kBjCoinA,   8 },
    { "Coin B",    0x38, 0, kBjCoinB,   8 },
    { "Cabinet",   0x80, 0, kBjCabinet, 2 },
};

inline constexpr DipSwitchBankDescriptor kBjDSW1 = {
    "DSW1", kBjDsw1Switches, 3
};

// ── DSW2 settings ───────────────────────────────────────────────────────

inline constexpr DipSetting kBjLives[] = {
    { "3",  0x00 },   // default
    { "4",  0x01 },
    { "5",  0x02 },
    { "2",  0x03 },
};

inline constexpr DipSetting kBjBirdSpeed[] = {
    { "Easy",    0x00 },
    { "Medium",  0x08 },   // default
    { "Hard",    0x10 },
    { "Hardest", 0x18 },
};

inline constexpr DipSetting kBjEnemiesSpeed[] = {
    { "Easy",    0x00 },
    { "Medium",  0x20 },   // default
    { "Hard",    0x40 },
    { "Hardest", 0x60 },
};

inline constexpr DipSetting kBjSpecialCoin[] = {
    { "Easy",  0x00 },
    { "Hard",  0x80 },   // default
};

inline constexpr DipSwitch kBjDsw2Switches[] = {
    { "Lives",           0x03, 0, kBjLives,        4 },
    { "Bird Speed",      0x18, 1, kBjBirdSpeed,    4 },
    { "Enemies Speed",   0x60, 1, kBjEnemiesSpeed, 4 },
    { "Special Coin",    0x80, 1, kBjSpecialCoin,  2 },
};

inline constexpr DipSwitchBankDescriptor kBjDSW2 = {
    "DSW2", kBjDsw2Switches, 4
};

} // namespace bombjack_constants
