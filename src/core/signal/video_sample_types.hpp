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

// Vector — Vectrex, Atari Asteroids/Tempest/Star Wars
// Beam position and intensity; no scanlines
struct alignas(8) VectorVideoSample {
    int16_t    x, y;
    uint8_t    intensity;
    uint8_t    _pad;
    VideoFlags flags;
    uint8_t    _pad2;
};
static_assert(sizeof(VectorVideoSample) == 8);
