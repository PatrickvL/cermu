#pragma once

// ============================================================================
// Namco Arcade Video Generator — TTL tile rendering with 90° rotation
// ============================================================================
//
// Models the discrete TTL video generation circuitry used by Namco arcade
// boards: Pac-Man (1980), Pengo (1982), and other similar arcade hardware.
//
// Display: 224×288 visible pixels (physically rotated 90° CW from a
// 288×224 monitor).  The VRAM stores a 32×32 tile grid; the video
// hardware applies a coordinate transform to produce the rotated output.
//
// Hardware:
//   VRAM (1 KB): tile indices in a 32×32 grid
//   Color RAM (1 KB): per-tile color attribute (6-bit)
//   Character ROM: Namco interleaved 2bpp format, 16 bytes per tile
//   Colortable PROM: maps (attr * 4 + pixel_2bit) → palette index
//
// VRAM-to-screen coordinate mapping (90° rotation):
//   VRAM rows 0-1:   bottom score strip → screen rows 34-35
//   VRAM rows 2-29:  main playfield     → screen cols 27..0 (reversed)
//   VRAM rows 30-31: top score strip    → screen rows 0-1
// ============================================================================

#include "core/signal/composite_video_out.hpp"
#include "core/signal/sync_flag.hpp"
#include "utils/tile_decoder.hpp"

#include <cstdint>
#include <cstring>

namespace namco_video_constants {
    // Signal dimensions after TTL address mapping (rotated 90° CW for arcade monitor).
    // Native raster is 288×224, but the address generation circuit maps VRAM entries
    // into a 224×288 output directly usable on the rotated display.
    // TODO(blocked): output native 288×224 when GPU-side rotation is available.
    //   Requires HardwareTraits rotation field + shader support.
    inline constexpr int WIDTH              = 224;   // visible width  (28 tile columns)
    inline constexpr int HEIGHT             = 288;   // visible height (36 tile rows)
    inline constexpr int TILE_SIZE          = 8;
    inline constexpr int VRAM_SIZE          = 1024;  // 32×32 grid
    inline constexpr int VRAM_COLS          = 32;
    inline constexpr int BYTES_PER_TILE     = 16;    // Namco 2bpp interleaved
    inline constexpr int VISIBLE_TILE_COLS  = 28;    // tile columns in output (224/8)
    inline constexpr int VISIBLE_TILE_ROWS  = 36;    // tile rows in output (288/8)
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
