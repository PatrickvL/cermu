#pragma once

// ============================================================================
// Video sample types — one per signal standard
// ============================================================================
//
// The sample type is the only thing that varies between signal standards.
// Each is a plain struct, sized and aligned for efficient store and compare.
// Chips include only the header for their specific sample type.
// ============================================================================

#include "core/signal/video_flags.hpp"
#include <cstdint>

// Composite / S-Video — VIC-II, TED, NES PPU, VIC-20 VIC
// color_index implies luma + chroma via chip DAC network
struct alignas(2) CompositeVideoSample {
    uint8_t    color_index;
    VideoFlags flags;
};
static_assert(sizeof(CompositeVideoSample) == 2);

// RGB — Amiga Denise, Atari ST shifter, later consoles
// Three independent DAC values; separate H and V sync
struct alignas(4) RGBVideoSample {
    uint8_t    r, g, b;
    VideoFlags flags;
};
static_assert(sizeof(RGBVideoSample) == 4);

// RGBI digital — C128 VDC, EGA
// 4-bit: R, G, B, I as individual TTL lines
struct alignas(2) RGBIVideoSample {
    uint8_t    rgbi;    // bits [3:0] = I, B, G, R
    VideoFlags flags;
};
static_assert(sizeof(RGBIVideoSample) == 2);

// Vector — Vectrex, Atari DVG/AVG (Asteroids, Tempest, Star Wars)
// Beam endpoint; renderer draws segment from previous sample position.
// BLANK flag suppresses draw but still moves beam (repositioning move).
// Monochrome systems (DVG, Battlezone, Gravitar) emit color_index == 0.
struct alignas(8) VectorVideoSample {
    int16_t    x, y;          // beam position, chip-native coordinate space
    uint8_t    intensity;     // Z-axis drive level; governs bloom, spot size
    uint8_t    color_index;   // AVG STAT[2:0] color select; 0 = white/mono
    VideoFlags flags;         // BLANK etc.
    uint8_t    _pad;
};
static_assert(sizeof(VectorVideoSample) == 8);
