#pragma once
/*
 * nes_palette.h — NES master palette with precalculated emphasis/greyscale
 *
 * Provides:
 *   - NES_COLOR_TABLE  — 64-entry base palette (0xFFBBGGRR ABGR)
 *   - NES_ATTENUATE_I  — integer attenuation factor for color emphasis
 *   - build_palette_cache() — fills a [16][64] cache covering every
 *     combination of the 3 emphasis bits and the greyscale bit.
 *
 * The cache index (0-15) is formed from PPUMASK:
 *     variant = ((mask >> 5) & 0x07) | ((mask & 0x01) << 3)
 *
 * On NTSC the emphasis bit order is (R, G, B); on PAL it is (G, R, B).
 * build_palette_cache() handles both via the is_pal flag.
 *
 * Output is 0xFFBBGGRR (ABGR, GL_RGBA little-endian convention).
 *
 * Header-only — palette LUTs are side-effect-free data per coding guidelines.
 */

#include <cstdint>

// ============================================================================
// Emphasis attenuation factor  (integer, applied as (c * ATTENUATE_I) >> 8)
// Real hardware attenuates de-emphasised channels to ~0.746 of full.
// 191 / 256 ≈ 0.74609 — within 0.01% of measured PPU attenuation.
// ============================================================================

inline constexpr uint16_t NES_ATTENUATE_I = 191;

// ============================================================================
// Base 64-color NES palette (0xFFBBGGRR — ABGR, little-endian GL convention)
// ============================================================================

inline constexpr uint32_t NES_COLOR_TABLE[64] = {
    0xFF666666, 0xFF882A00, 0xFFA71214, 0xFFA4003B, 0xFF7E005C, 0xFF40006E, 0xFF00066C, 0xFF001D56,
    0xFF003533, 0xFF00480B, 0xFF005200, 0xFF084F00, 0xFF4D4000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFADADAD, 0xFFD95F15, 0xFFFF4042, 0xFFFE2775, 0xFFCC1AA0, 0xFF7B1EB7, 0xFF2031B5, 0xFF004E99,
    0xFF006D6B, 0xFF008738, 0xFF00930C, 0xFF328F00, 0xFF8D7C00, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFFB064, 0xFFFF9092, 0xFFFF76C6, 0xFFFF6AF3, 0xFFCC6EFF, 0xFF7081FF, 0xFF129CFF,
    0xFF00B0D7, 0xFF00C1A6, 0xFF00C979, 0xFF8ACA5A, 0xFFEAC04B, 0xFF424242, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFFDFC0, 0xFFFFD2D3, 0xFFFFC8E8, 0xFFFFC2FB, 0xFFEAC4FE, 0xFFC5CCFE, 0xFFA5D8F7,
    0xFF94E5E4, 0xFF96EFCF, 0xFFABF4BD, 0xFFCCF3B3, 0xFFF2EBB5, 0xFFB8B8B8, 0xFF000000, 0xFF000000,
};

// ============================================================================
// Precalculated palette cache builder
// ============================================================================
//
// Builds a 16×64 uint32_t (ABGR) lookup table covering every combination of
// the 3 emphasis bits (PPUMASK [7:5]) and the greyscale bit (PPUMASK [0]).
//
// Each emphasis bit attenuates the OTHER two color channels.  When a
// channel is NOT attenuated it passes through at full intensity (full_*).
//
// On PAL hardware, R and G emphasis outputs are swapped relative to NTSC.
// The `is_pal` flag adjusts the bit-to-channel mapping accordingly.
//
// Greyscale mode (bit 3 of variant) masks the 6-bit palette index to the
// grey column ($x0) by ANDing with 0x30 instead of 0x3F.

inline void build_palette_cache(bool is_pal, uint32_t cache[16][64]) {
    const int r_bit = is_pal ? 1 : 0;
    const int g_bit = is_pal ? 0 : 1;

    for (uint8_t variant = 0; variant < 16; ++variant) {
        const bool full_r = !((variant >> g_bit) & 1) & !((variant >> 2) & 1);
        const bool full_g = !((variant >> r_bit) & 1) & !((variant >> 2) & 1);
        const bool full_b = !((variant >> r_bit) & 1) & !((variant >> g_bit) & 1);

        const uint8_t idx_mask = ((variant >> 3) & 1) ? 0x30 : 0x3F;
        uint32_t*     dst      = cache[variant];

        for (int i = 0; i < 64; ++i) {
            const uint32_t p = NES_COLOR_TABLE[i & idx_mask];
            const uint8_t r = full_r ? (p & 0xFF)         : (uint8_t)(((p & 0xFF) * NES_ATTENUATE_I) >> 8);
            const uint8_t g = full_g ? ((p >> 8) & 0xFF)  : (uint8_t)((((p >> 8) & 0xFF) * NES_ATTENUATE_I) >> 8);
            const uint8_t b = full_b ? ((p >> 16) & 0xFF) : (uint8_t)((((p >> 16) & 0xFF) * NES_ATTENUATE_I) >> 8);
            dst[i] = 0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
        }
    }
}


