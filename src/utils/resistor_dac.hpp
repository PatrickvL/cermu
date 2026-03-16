#pragma once

#include <cstdint>

// ============================================================================
// Resistor-Weighted DAC Palette Decode
// ============================================================================
//
// Many arcade and home-computer video systems use resistor-weighted DACs to
// convert a few digital bits per channel into analog RGB voltages.  The
// weight of each bit is determined by its resistor value in the DAC ladder.
//
// This header provides constexpr helpers that replicate common DAC circuits,
// eliminating the identical code duplicated across Namco, BombJack, and
// similar systems.
//
// All functions return ABGR-format uint32_t (0xFFBBGGRR) — the standard
// framebuffer format for OpenGL GL_RGBA8 on little-endian.
// ============================================================================

namespace resistor_dac {

/// Build an ABGR color from 8-bit R, G, B components.
inline constexpr uint32_t make_abgr(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u
         | (static_cast<uint32_t>(b) << 16)
         | (static_cast<uint32_t>(g) << 8)
         | static_cast<uint32_t>(r);
}

// ============================================================================
// Common 3-3-2 bit DAC (used by Namco Pac-Man, BombJack, and many others)
// ============================================================================
//
// Byte layout:
//   bits [2:0] = red   (3-bit, weights 0x21/0x47/0x97)
//   bits [5:3] = green (3-bit, weights 0x21/0x47/0x97)
//   bits [7:6] = blue  (2-bit, weights 0x51/0xAE)
//
// This is the most common arcade palette DAC.  The resistor values produce
// the following per-bit contributions:
//   bit 0: 0x21 (33),  bit 1: 0x47 (71),  bit 2: 0x97 (151)  (R and G)
//   bit 0: 0x51 (81),  bit 1: 0xAE (174)                      (B)

inline constexpr uint32_t decode_3_3_2(uint8_t entry) {
    int r = 0x21 * ((entry >> 0) & 1) + 0x47 * ((entry >> 1) & 1) + 0x97 * ((entry >> 2) & 1);
    int g = 0x21 * ((entry >> 3) & 1) + 0x47 * ((entry >> 4) & 1) + 0x97 * ((entry >> 5) & 1);
    int b = 0x51 * ((entry >> 6) & 1) + 0xAE * ((entry >> 7) & 1);
    return make_abgr(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
}

} // namespace resistor_dac
