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

// ── Memory ──────────────────────────────────────────────────────────────
inline constexpr uint16_t PACMAN_ROM_SIZE      = 0x4000;    // 16 KB (4 × 4 KB)
inline constexpr uint16_t PENGO_ROM_SIZE       = 0x8000;    // 32 KB (8 × 4 KB)

// ── Display ─────────────────────────────────────────────────────────────
// Physical display is rotated 90° CW: 288 tall × 224 wide → 224×288 in scanline order
inline constexpr int FB_WIDTH                  = 224;
inline constexpr int FB_HEIGHT                 = 288;

// Palette
inline constexpr int PALETTE_ENTRIES           = 32;

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
