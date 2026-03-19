#pragma once

// ============================================================================
// Bomb Jack Video Generator — foreground tile rendering hardware
// ============================================================================
//
// Models the custom TTL video circuitry of the Bomb Jack arcade board (1984).
//
// Display: 256×224 visible pixels, 32×28 foreground tiles (8×8 each).
//
// Hardware:
//   FG tilemap RAM (1 KB at $9000): tile index per cell (32×32 grid)
//   FG attribute RAM (1 KB at $9400): palette group + flip flags per cell
//   Character ROM: 3bpp planar format, 24 bytes per tile
//   Palette RAM: 128 entries, decoded via resistor DAC
//
// Attribute byte format:
//   bits [3:0] = palette group (selects 8 colors from 128-entry palette)
//   bit 6 = flip X
//   bit 7 = flip Y
//
// Only the foreground tilemap is rendered here; background and sprite
// layers are not yet implemented.
// ============================================================================

#include "core/signal/composite_video_stream.hpp"
#include "core/signal/video_flags.hpp"
#include "utils/tile_decoder.hpp"

#include <cstdint>
#include <cstring>

namespace bombjack_video_constants {
    inline constexpr int WIDTH       = 256;
    inline constexpr int HEIGHT      = 224;
    inline constexpr int TILE_COLS   = 32;
    inline constexpr int TILE_ROWS   = 28;  // 28 of 32 rows visible
    inline constexpr int TILE_SIZE   = 8;
    inline constexpr int BPP         = 3;
    inline constexpr int BYTES_PER_TILE = 24;  // 3 planes × 8 rows
}

struct BombJackVideo {
    // --- Configuration (set once at init) ---
    void set_stream(CompositeVideoStream* s) { video_stream_ = s; }
    void set_char_rom(const uint8_t* rom, int size) {
        char_rom_ = rom;
        char_count_ = size / bombjack_video_constants::BYTES_PER_TILE;
    }

    // --- Per-frame dynamic state (set by system before render_frame) ---
    void set_tilemap(const uint8_t* tilemap) { tilemap_ = tilemap; }
    void set_attr_map(const uint8_t* attr) { attr_map_ = attr; }

    // --- Render one frame ---
    void render_frame();

private:
    void drive_stream();

    CompositeVideoStream* video_stream_ = nullptr;
    const uint8_t*        char_rom_     = nullptr;
    const uint8_t*        tilemap_      = nullptr;
    const uint8_t*        attr_map_     = nullptr;
    int                   char_count_   = 0;
    uint8_t               pixel_buf_[bombjack_video_constants::WIDTH * bombjack_video_constants::HEIGHT] = {};
};
