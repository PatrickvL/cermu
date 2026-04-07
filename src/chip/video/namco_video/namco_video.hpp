#pragma once

// ============================================================================
// Namco Arcade Video Generator — TTL tile rendering (native 288×224 raster)
// ============================================================================
//
// Models the discrete TTL video generation circuitry used by Namco arcade
// boards: Pac-Man (1980), Pengo (1982), and other similar arcade hardware.
//
// Display: 288×224 native raster (36×28 tiles).  The physical arcade
// monitor is rotated 90° CW, presenting a portrait display to the player.
// GPU-side rotation is handled by the CRT shader via DisplayRotation::CW90
// in the system's HardwareTraits.
//
// Hardware:
//   VRAM (1 KB): tile indices in a 32×32 grid
//   Color RAM (1 KB): per-tile color attribute (6-bit)
//   Character ROM: Namco interleaved 2bpp format, 16 bytes per tile
//   Colortable PROM: maps (attr * 4 + pixel_2bit) → palette index
//
// VRAM-to-native-raster tile mapping:
//   nx ∈ [0,1]:   left score strip  → VRAM row = 29-ny, col = nx+30
//   nx ∈ [2,33]:  main playfield    → VRAM row = nx-2,  col = ny+2
//   nx ∈ [34,35]: right score strip → VRAM row = 29-ny, col = nx-34
// ============================================================================

#include "core/signal/composite_video_out.hpp"
#include "core/signal/sync_flag.hpp"
#include "utils/tile_decoder.hpp"

#include <cstdint>
#include <cstring>

namespace namco_video_constants {
    // Native raster dimensions — 288×224 (36×28 tiles).
    // The physical arcade monitor is rotated 90° CW, presenting a portrait
    // display to the player.  The shader handles this rotation via the
    // DisplayRotation::CW90 trait; the video generator outputs unrotated.
    inline constexpr int WIDTH              = 288;   // native width  (36 tile columns)
    inline constexpr int HEIGHT             = 224;   // native height (28 tile rows)
    inline constexpr int TILE_SIZE          = 8;
    inline constexpr int VRAM_SIZE          = 1024;  // 32×32 grid
    inline constexpr int VRAM_COLS          = 32;
    inline constexpr int BYTES_PER_TILE     = 16;    // Namco 2bpp interleaved
    inline constexpr int VISIBLE_TILE_COLS  = 36;    // tile columns in native output (288/8)
    inline constexpr int VISIBLE_TILE_ROWS  = 28;    // tile rows in native output (224/8)
}

struct NamcoVideo {
    // --- Configuration (set once at init) ---
    void set_video_out(CompositeVideoOut* s) { video_out_ = s; }
    void set_char_rom(const uint8_t* rom, int size) {
        char_rom_ = rom;
        char_count_ = size / namco_video_constants::BYTES_PER_TILE;
    }
    void set_colortable_prom(const uint8_t* prom) { colortable_ = prom; }

    // --- Per-frame dynamic state (set by system before render_frame) ---
    void set_vram(const uint8_t* vram) { vram_ = vram; }
    void set_cram(const uint8_t* cram) { cram_ = cram; }

    // --- Render one frame ---
    void render_frame();

private:
    void drive_video_out();

    CompositeVideoOut* video_out_ = nullptr;
    const uint8_t*        char_rom_     = nullptr;
    const uint8_t*        colortable_   = nullptr;
    const uint8_t*        vram_         = nullptr;
    const uint8_t*        cram_         = nullptr;
    int                   char_count_   = 0;
    uint8_t               pixel_buf_[namco_video_constants::WIDTH * namco_video_constants::HEIGHT] = {};
};
