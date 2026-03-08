#pragma once
/*
 * namco_arcade_constants.h — Namco Pac-Man / Pengo arcade hardware constants
 *
 * Pac-Man (Namco, 1980) and Pengo (Sega/Coreland, 1982) share very similar
 * hardware based on the "Namco Pac-Man" arcade board:
 *   CPU:   Z80A @ 3.072 MHz (18.432 MHz crystal ÷ 6)
 *   Sound: Namco WSG3 — 3-voice wavetable sound generator
 *   Video: 288×224 (logically 36×28 tiles, 8×8, rotated 90°)
 *          Tilemap layer + 8 sprites (16×16)
 *          Palette: 32 colors from a 16-entry palette table
 *   RAM:   2 KB main + 1 KB video + 1 KB color
 *
 * Pengo differences:
 *   - 8 KB character ROM (vs 4 KB for Pac-Man)
 *   - Different ROM layout and memory map
 *   - Additional encryption on some ROM chips
 *   - Slightly different color palette encoding
 */

#include <cstdint>

namespace namco_arcade_constants {

// ── CPU ─────────────────────────────────────────────────────────────────
inline constexpr uint32_t CPU_FREQ_HZ          = 3072000;   // 18.432 MHz ÷ 6

// ── Memory (Pac-Man layout) ─────────────────────────────────────────────
inline constexpr uint16_t ROM_BASE             = 0x0000;
inline constexpr uint16_t PACMAN_ROM_SIZE      = 0x4000;    // 16 KB (4 × 4 KB)
inline constexpr uint16_t PENGO_ROM_SIZE       = 0x8000;    // 32 KB (8 × 4 KB)

inline constexpr uint16_t RAM_BASE             = 0x4000;    // Pac-Man: $4000
inline constexpr uint16_t RAM_SIZE             = 0x0400;    // 1 KB main RAM

// Video RAM and color RAM (tile + sprite attribute tables)
inline constexpr uint16_t VIDEO_RAM_BASE       = 0x4000;    // $4000–$43FF (Pac-Man)
inline constexpr uint16_t VIDEO_RAM_SIZE       = 0x0400;    // 1 KB tilemap
inline constexpr uint16_t COLOR_RAM_BASE       = 0x4400;    // $4400–$47FF
inline constexpr uint16_t COLOR_RAM_SIZE       = 0x0400;    // 1 KB color attributes

// Sprite RAM (within main RAM space, last 16 bytes)
inline constexpr uint16_t SPRITE_POS_BASE      = 0x5060;    // Sprite X/Y positions
inline constexpr uint16_t SPRITE_ATTR_BASE     = 0x4FF0;    // Sprite tile + flip

// ── I/O (memory-mapped) ─────────────────────────────────────────────────
inline constexpr uint16_t INT_ENABLE           = 0x5000;    // Bit 0: enable VBLANK IRQ
inline constexpr uint16_t SOUND_ENABLE         = 0x5001;    // Bit 0: enable sound
inline constexpr uint16_t FLIP_SCREEN          = 0x5003;    // Cocktail flip
inline constexpr uint16_t WATCHDOG             = 0x5007;    // Watchdog reset

// Input ports (active low)
inline constexpr uint16_t IN0                  = 0x5000;    // Joystick + coin switches
inline constexpr uint16_t IN1                  = 0x5040;    // Player 2 + start buttons
inline constexpr uint16_t DSW1                 = 0x5080;    // DIP switches

// Sound register base ($5040–$5060)
inline constexpr uint16_t WSG_BASE             = 0x5040;    // 0x20 bytes: 3 voices × frequency/volume/wave
inline constexpr uint16_t WSG_SIZE             = 0x0020;

// ── Display ─────────────────────────────────────────────────────────────
// Physical display is rotated 90° CW: 288 tall × 224 wide → 224×288 in scanline order
inline constexpr int DISPLAY_WIDTH             = 224;       // After rotation
inline constexpr int DISPLAY_HEIGHT            = 288;
inline constexpr int FB_WIDTH                  = 224;
inline constexpr int FB_HEIGHT                 = 288;

inline constexpr int TILE_SIZE                 = 8;
inline constexpr int TILES_X                   = 28;        // 224 / 8
inline constexpr int TILES_Y                   = 36;        // 288 / 8

inline constexpr int SPRITE_COUNT              = 8;          // 8 hardware sprites
inline constexpr int SPRITE_SIZE               = 16;         // 16×16 pixels

// Palette
inline constexpr int PALETTE_ENTRIES           = 32;
inline constexpr int COLOR_TABLE_SIZE          = 16;         // 16-entry color LUT

// ── Timing ──────────────────────────────────────────────────────────────
inline constexpr int REFRESH_HZ               = 60;          // ~60.61 Hz actual
inline constexpr uint32_t TSTATES_PER_FRAME   = CPU_FREQ_HZ / REFRESH_HZ;  // ~51200
inline constexpr int VBLANK_SCANLINE          = 224;         // VBLANK IRQ at end of visible area
inline constexpr int DEFAULT_SAMPLE_RATE      = 44100;

// ── ROM counts ──────────────────────────────────────────────────────────
inline constexpr int PACMAN_CHAR_ROM_SIZE      = 0x1000;    // 4 KB character ROM
inline constexpr int PENGO_CHAR_ROM_SIZE       = 0x2000;    // 8 KB character ROM
inline constexpr int SPRITE_ROM_SIZE           = 0x1000;    // 4 KB sprite ROM
inline constexpr int PALETTE_PROM_SIZE         = 0x0020;    // 32 bytes palette PROM
inline constexpr int COLORTABLE_PROM_SIZE      = 0x0100;    // 256 bytes color table PROM
inline constexpr int WAVEFORM_ROM_SIZE         = 0x0100;    // 256 bytes (8 waveforms × 32 samples)

} // namespace namco_arcade_constants
