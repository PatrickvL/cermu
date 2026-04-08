#pragma once

// ============================================================================
// Bomb Jack Video Generator — 3-layer TTL video rendering hardware
// ============================================================================
//
// Models the custom TTL video circuitry of the Bomb Jack arcade board (1984).
//
// Display: 256×224 visible pixels, composed from 3 layers:
//   1. Background: 16×16 tiles (16×16 grid = 256 entries per image)
//      Selected by BG image register ($9E00), tile data in dedicated ROMs.
//      Bit 4 of select register must be set for image to be visible.
//      Tiles can be Y-flipped (attribute bit 7).
//   2. Foreground: 8×8 tiles (32×28 visible from 32×32 grid)
//      512 tile codes (tile byte + attr bit 4 as 9th bit).
//      Transparency: pixel value 0 shows background through.
//   3. Sprites: 24 hardware sprites, 16×16 or 32×32 pixels
//      Attributes at $9820–$987F (4 bytes per sprite).
//      Sprite 0 has highest priority (rendered last → on top).
//      Transparency: pixel value 0 shows layers below.
//
// All layers use the same 3bpp tile decoder: 3 planes → 3-bit pixel,
// combined with 4-bit palette group from attribute → 7-bit color index
// into a shared 128-entry palette.
//
// Palette: 128 entries × 16-bit (xxxxBBBB_GGGGRRRR), 256 bytes at $9C00.
// ============================================================================

#include "core/signal/composite_video_out.hpp"
#include "core/signal/sync_flag.hpp"

#include <cstdint>
#include <cstring>

namespace bombjack_video_constants {
    inline constexpr int WIDTH       = 256;
    inline constexpr int HEIGHT      = 224;
    inline constexpr int TILE_COLS   = 32;
    inline constexpr int TILE_ROWS   = 28;  // 28 of 32 rows visible
    inline constexpr int TILE_SIZE   = 8;
    inline constexpr int BPP         = 3;
    inline constexpr int BYTES_PER_TILE = 24;  // 3 planes × 8 rows (FG)

    // The hardware renders a 256×256 tile space but the monitor shows only
    // 256×224 starting at pixel row 16 (MAME visarea: 0,255,16,239).
    // All layers must subtract this offset when mapping to the framebuffer.
    inline constexpr int VISIBLE_Y_START = 16;  // 2 tile rows clipped at top
}

struct BombJackVideo {

    // --- Configuration (set once at init) ---
    void set_video_out(CompositeVideoOut* s) { video_out_ = s; }

    // Foreground character ROMs (3 planes, each 4KB)
    void set_char_rom(const uint8_t* rom, int size) {
        char_rom_ = rom;
        char_rom_size_ = size;
    }

    // Background tile ROMs (3 planes, each 8KB — interleaved)
    void set_bg_tile_rom(const uint8_t* p0, const uint8_t* p1, const uint8_t* p2) {
        bg_tile_rom_[0] = p0; bg_tile_rom_[1] = p1; bg_tile_rom_[2] = p2;
    }

    // Background map ROM (4KB: 8 images × 512 bytes each)
    void set_bg_map_rom(const uint8_t* rom) { bg_map_rom_ = rom; }

    // Sprite ROMs (3 planes, each 8KB — interleaved)
    void set_sprite_rom(const uint8_t* p0, const uint8_t* p1, const uint8_t* p2) {
        sprite_rom_[0] = p0; sprite_rom_[1] = p1; sprite_rom_[2] = p2;
    }

    // --- Per-frame dynamic state (set by system before render_frame) ---
    void set_tilemap(const uint8_t* tilemap) { tilemap_ = tilemap; }
    void set_attr_map(const uint8_t* attr) { attr_map_ = attr; }
    void set_sprite_ram(const uint8_t* spr) { sprite_ram_ = spr; }
    void set_bg_image_select(uint8_t sel) { bg_image_select_ = sel; }

    // --- Render one frame (all 3 layers) ---
    void render_frame();

private:
    void render_background();
    void render_foreground();
    void render_sprites();
    void drive_video_out();

    static constexpr int WIDTH  = bombjack_video_constants::WIDTH;
    static constexpr int HEIGHT = bombjack_video_constants::HEIGHT;

    CompositeVideoOut* video_out_ = nullptr;

    // Foreground
    const uint8_t* char_rom_    = nullptr;
    const uint8_t* tilemap_     = nullptr;
    const uint8_t* attr_map_    = nullptr;
    int            char_rom_size_ = 0;

    // Background
    const uint8_t* bg_tile_rom_[3] = {};
    const uint8_t* bg_map_rom_     = nullptr;
    uint8_t        bg_image_select_ = 0;

    // Sprites
    const uint8_t* sprite_rom_[3] = {};
    const uint8_t* sprite_ram_    = nullptr;

    uint8_t pixel_buf_[WIDTH * HEIGHT] = {};
};
