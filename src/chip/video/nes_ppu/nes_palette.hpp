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
#include "chip/video/video_chip_base.hpp"

// ============================================================================
// Emphasis attenuation factor  (integer, applied as (c * ATTENUATE_I) >> 8)
// Real hardware attenuates de-emphasised channels to ~0.746 of full.
// 191 / 256 ≈ 0.74609 — within 0.01% of measured PPU attenuation.
// ============================================================================

inline constexpr uint16_t NES_ATTENUATE_I = 191;

// ============================================================================
// Base 64-color NES palettes (0xFFBBGGRR — ABGR, little-endian GL convention)
// ============================================================================

// Default palette (community-derived 2C02 decode)
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

// Smooth (FBX) — widely used community palette by FirebrandX
inline constexpr uint32_t NES_COLOR_TABLE_FBX[64] = {
    0xFF616161, 0xFF880000, 0xFF990D1F, 0xFF791337, 0xFF5D0062, 0xFF1E0079, 0xFF000E6C, 0xFF00234D,
    0xFF003522, 0xFF00480B, 0xFF004E00, 0xFF174000, 0xFF4F3200, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFADADAD, 0xFFD44401, 0xFFFF3518, 0xFFFB2459, 0xFFCC18A1, 0xFF6B23C5, 0xFF1E37BE, 0xFF00519E,
    0xFF006D6A, 0xFF008335, 0xFF009010, 0xFF2E8A00, 0xFF7E7800, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFF9B5E, 0xFFFF7F85, 0xFFFF68B5, 0xFFFF5CF2, 0xFFCE61FF, 0xFF7E75FF, 0xFF3E8CFF,
    0xFF0DA8DF, 0xFF10C0AD, 0xFF1ACC7B, 0xFF63C963, 0xFFBABC51, 0xFF4E4E4E, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFFD4BB, 0xFFFFC7CE, 0xFFFFBEE3, 0xFFFFB9F7, 0xFFEDBCFF, 0xFFC5C5FF, 0xFFA5D0FC,
    0xFF93DCEC, 0xFF94E8D8, 0xFF9CEDC6, 0xFFB7ECC0, 0xFFDAE7BB, 0xFFB3B3B3, 0xFF000000, 0xFF000000,
};

// Wavebeam — by Nakedarthur, attempts to match measured 2C02 output
inline constexpr uint32_t NES_COLOR_TABLE_WAVEBEAM[64] = {
    0xFF6B6B6B, 0xFF8C1800, 0xFFA41318, 0xFF8E1746, 0xFF6A0C6A, 0xFF300E76, 0xFF001468, 0xFF002E4F,
    0xFF00422A, 0xFF005309, 0xFF005B00, 0xFF204C00, 0xFF4C3B00, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFB4B4B4, 0xFFE03800, 0xFFFC3041, 0xFFF31E76, 0xFFCE13B3, 0xFF7B1DD5, 0xFF2932C8, 0xFF004DA4,
    0xFF00697B, 0xFF00814B, 0xFF008E20, 0xFF348A08, 0xFF7F7800, 0xFF050505, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFF9763, 0xFFFF7F8A, 0xFFFF6AB5, 0xFFFF61E3, 0xFFCC65FF, 0xFF7D77FD, 0xFF3E92D9,
    0xFF19ABB2, 0xFF12C084, 0xFF2BCC5B, 0xFF69C73E, 0xFFB3B838, 0xFF3C3C3C, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFFD2BC, 0xFFFFC7CF, 0xFFFFBFE3, 0xFFFFBBF6, 0xFFEABDFF, 0xFFC4C8FF, 0xFFA7D3F9,
    0xFF95DFE8, 0xFF92EBD4, 0xFF9FF0C2, 0xFFB7EDBA, 0xFFDBE7B9, 0xFFAAAAAA, 0xFF000000, 0xFF000000,
};

// Nestopia — classic Nestopia emulator palette
inline constexpr uint32_t NES_COLOR_TABLE_NESTOPIA[64] = {
    0xFF656565, 0xFF7E2900, 0xFF901300, 0xFF880030, 0xFF640064, 0xFF270077, 0xFF000D6F, 0xFF002455,
    0xFF00352E, 0xFF00440A, 0xFF004E00, 0xFF164200, 0xFF463300, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFADADAD, 0xFFD36100, 0xFFF7480B, 0xFFF43548, 0xFFCC278C, 0xFF7D2CB8, 0xFF2640BF, 0xFF0058A0,
    0xFF006F72, 0xFF008441, 0xFF00901A, 0xFF338906, 0xFF847800, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFFAF5E, 0xFFFF9988, 0xFFFF80B6, 0xFFFF73EC, 0xFFCC78FF, 0xFF7A8BFF, 0xFF3BA2FF,
    0xFF10B6EC, 0xFF18CAB6, 0xFF30D382, 0xFF6CCD57, 0xFFBEC13F, 0xFF424242, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFFFDDB9, 0xFFFFD2CA, 0xFFFFC8DD, 0xFFFFC4F1, 0xFFF0C6FF, 0xFFCAD0FF, 0xFFB0DAFF,
    0xFFA1E5F8, 0xFFA3EEE7, 0xFFAFF3D6, 0xFFC3F1CA, 0xFFE7EABF, 0xFFB8B8B8, 0xFF000000, 0xFF000000,
};

// Named palette registry for NES PPU
inline constexpr int NES_NAMED_PALETTE_COUNT = 4;

// Cannot be constexpr because NamedPalette has pointer members to non-constexpr data
inline const NamedPalette NES_NAMED_PALETTES[NES_NAMED_PALETTE_COUNT] = {
    { "default",  "Default (2C02)",  NES_COLOR_TABLE,          64 },
    { "fbx",      "Smooth (FBX)",    NES_COLOR_TABLE_FBX,      64 },
    { "wavebeam", "Wavebeam",        NES_COLOR_TABLE_WAVEBEAM, 64 },
    { "nestopia", "Nestopia",        NES_COLOR_TABLE_NESTOPIA,  64 },
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

inline void build_palette_cache(bool is_pal, uint32_t cache[16][64],
                               const uint32_t base[64] = NES_COLOR_TABLE) {
    const int r_bit = is_pal ? 1 : 0;
    const int g_bit = is_pal ? 0 : 1;

    for (uint8_t variant = 0; variant < 16; ++variant) {
        const bool full_r = !((variant >> g_bit) & 1) & !((variant >> 2) & 1);
        const bool full_g = !((variant >> r_bit) & 1) & !((variant >> 2) & 1);
        const bool full_b = !((variant >> r_bit) & 1) & !((variant >> g_bit) & 1);

        const uint8_t idx_mask = ((variant >> 3) & 1) ? 0x30 : 0x3F;
        uint32_t*     dst      = cache[variant];

        for (int i = 0; i < 64; ++i) {
            const uint32_t p = base[i & idx_mask];
            const uint8_t r = full_r ? (p & 0xFF)         : (uint8_t)(((p & 0xFF) * NES_ATTENUATE_I) >> 8);
            const uint8_t g = full_g ? ((p >> 8) & 0xFF)  : (uint8_t)((((p >> 8) & 0xFF) * NES_ATTENUATE_I) >> 8);
            const uint8_t b = full_b ? ((p >> 16) & 0xFF) : (uint8_t)((((p >> 16) & 0xFF) * NES_ATTENUATE_I) >> 8);
            dst[i] = 0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
        }
    }
}


