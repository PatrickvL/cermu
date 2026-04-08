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
inline constexpr uint16_t DSW1                 = 0xB004;    // DIP switch bank 1 (read; write = flip screen)
inline constexpr uint16_t DSW2                 = 0xB005;    // DIP switch bank 2 (read-only)
inline constexpr uint16_t SOUND_LATCH          = 0xB800;    // Write → sound CPU command
inline constexpr uint16_t BG_SELECT            = 0x9E00;    // Background image select (write)
inline constexpr uint16_t NMI_MASK             = 0xB000;    // Write: NMI enable mask
inline constexpr uint16_t FLIP_SCREEN          = 0xB004;    // Write: flip screen

// ── Player input bits (active high: 1 = pressed) ───────────────────────
// P1/P2 share the same layout
inline constexpr uint8_t INPUT_RIGHT           = 0x01;
inline constexpr uint8_t INPUT_LEFT            = 0x02;
inline constexpr uint8_t INPUT_UP              = 0x04;
inline constexpr uint8_t INPUT_DOWN            = 0x08;
inline constexpr uint8_t INPUT_BUTTON1         = 0x10;      // Jump

// SYSTEM port ($B002)
inline constexpr uint8_t SYSTEM_COIN1          = 0x01;
inline constexpr uint8_t SYSTEM_COIN2          = 0x02;
inline constexpr uint8_t SYSTEM_START1         = 0x04;      // 1P Start
inline constexpr uint8_t SYSTEM_START2         = 0x08;      // 2P Start

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
// DSW1 ($B004 read) — coinage, lives, cabinet, demo sounds
// DSW2 ($B005 read) — bonus life, difficulty settings
//
// Bit patterns and defaults match MAME bombjack.cpp (mamedev/mame).
// All active-high: factory default bits produce the default_index setting.
//
// DSW1 default = 0xC0  (Cabinet=Upright, Demo_Sounds=On, rest=0)
// DSW2 default = 0x50  (Bird_Speed=Hard, Enemies=Hard, rest=0)

// ── DSW1 settings ───────────────────────────────────────────────────────

inline constexpr DipSetting kBjCoinA[] = {
    { "1 Coin / 1 Credit",   0x00 },
    { "1 Coin / 2 Credits",  0x01 },
    { "1 Coin / 3 Credits",  0x02 },
    { "1 Coin / 6 Credits",  0x03 },
};

inline constexpr DipSetting kBjCoinB[] = {
    { "2 Coins / 1 Credit",  0x04 },
    { "1 Coin / 1 Credit",   0x00 },
    { "1 Coin / 2 Credits",  0x08 },
    { "1 Coin / 3 Credits",  0x0C },
};

inline constexpr DipSetting kBjLives[] = {
    { "2",  0x30 },
    { "3",  0x00 },
    { "4",  0x10 },
    { "5",  0x20 },
};

inline constexpr DipSetting kBjCabinet[] = {
    { "Upright",   0x40 },
    { "Cocktail",  0x00 },
};

inline constexpr DipSetting kBjDemoSounds[] = {
    { "Off",  0x00 },
    { "On",   0x80 },
};

inline constexpr DipSwitch kBjDsw1Switches[] = {
    { "Coin A",       0x03, 0, kBjCoinA,      4 },
    { "Coin B",       0x0C, 1, kBjCoinB,      4 },
    { "Lives",        0x30, 1, kBjLives,      4 },
    { "Cabinet",      0x40, 0, kBjCabinet,    2 },
    { "Demo Sounds",  0x80, 1, kBjDemoSounds, 2 },
};

inline constexpr DipSwitchBankDescriptor kBjDSW1 = {
    "DSW1", kBjDsw1Switches, 5
};

// ── DSW2 settings ───────────────────────────────────────────────────────

inline constexpr DipSetting kBjBonusLife[] = {
    { "Every 30k",            0x02 },
    { "Every 100k",           0x01 },
    { "50k, 100k and 300k",   0x07 },
    { "50k and 100k",         0x05 },
    { "50k only",             0x03 },
    { "100k and 300k",        0x06 },
    { "100k only",            0x04 },
    { "None",                 0x00 },
};

inline constexpr DipSetting kBjBirdSpeed[] = {
    { "Easy",    0x00 },
    { "Medium",  0x08 },
    { "Hard",    0x10 },
    { "Hardest", 0x18 },
};

inline constexpr DipSetting kBjEnemiesSpeed[] = {
    { "Easy",    0x20 },
    { "Medium",  0x00 },
    { "Hard",    0x40 },
    { "Hardest", 0x60 },
};

inline constexpr DipSetting kBjSpecialCoin[] = {
    { "Easy",  0x00 },
    { "Hard",  0x80 },
};

inline constexpr DipSwitch kBjDsw2Switches[] = {
    { "Bonus Life (Unused)",    0x07, 7, kBjBonusLife,    8 },
    { "Bird Speed",             0x18, 2, kBjBirdSpeed,    4 },
    { "Enemies Number & Speed", 0x60, 2, kBjEnemiesSpeed, 4 },
    { "Special Coin",           0x80, 0, kBjSpecialCoin,  2 },
};

inline constexpr DipSwitchBankDescriptor kBjDSW2 = {
    "DSW2", kBjDsw2Switches, 4
};

} // namespace bombjack_constants
