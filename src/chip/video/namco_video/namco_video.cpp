#include "chip/video/namco_video/namco_video.hpp"

using namespace namco_video_constants;

// ============================================================================
// Native 288×224 raster output
// ============================================================================
//
// Outputs the native (unrotated) 288×224 raster.  The physical arcade
// monitor is rotated 90° CW; GPU-side rotation is handled by the CRT
// shader via DisplayRotation::CW90 in HardwareTraits.
//
// Native tile (nx, ny) → VRAM offset mapping:
//   nx ∈ [0,1]:   left score strip  → VRAM row = 29-ny, col = nx+30
//   nx ∈ [2,33]:  main playfield    → VRAM row = nx-2,  col = ny+2
//   nx ∈ [34,35]: right score strip → VRAM row = 29-ny, col = nx-34

void NamcoVideo::render_frame() {
    if (!vram_ || !cram_ || !char_rom_ || !colortable_ || !video_out_) return;

    std::memset(pixel_buf_, 0, sizeof(pixel_buf_));

    // Render all tiles in native raster order (36 columns × 28 rows)
    for (int ny = 0; ny < VISIBLE_TILE_ROWS; ny++) {
        for (int nx = 0; nx < VISIBLE_TILE_COLS; nx++) {
            int mx, my;
            if (nx < 2) {
                // Left score strip (player sees right side after CW90 rotation)
                mx = 29 - ny;
                my = nx + 30;
            } else if (nx >= 34) {
                // Right score strip (player sees left side after CW90 rotation)
                mx = 29 - ny;
                my = nx - 34;
            } else {
                // Main playfield
                mx = nx - 2;
                my = ny + 2;
            }

            if (mx < 0 || mx >= VRAM_COLS || my < 0 || my >= VRAM_COLS)
                continue;

            int offs = my * VRAM_COLS + mx;
            uint8_t tile_idx = vram_[offs];
            uint8_t color_attr = cram_[offs] & 0x3F;

            if (tile_idx >= char_count_) tile_idx = 0;

            const uint8_t* tile = char_rom_ + tile_idx * BYTES_PER_TILE;

            int fb_x = nx * TILE_SIZE;
            int fb_y = ny * TILE_SIZE;
            uint8_t* dst = pixel_buf_ + fb_y * WIDTH + fb_x;
            tile_decoder::decode_namco_tile(tile, dst, WIDTH,
                                            colortable_, color_attr);
        }
    }

    drive_video_out();
}

void NamcoVideo::drive_video_out() {
    if (!video_out_) return;
    for (int y = 0; y < HEIGHT; y++) {
        const uint8_t* line = pixel_buf_ + y * WIDTH;
        video_out_->drive({0, SyncFlag::HSync});
        for (int x = 0; x < WIDTH; x++) {
            video_out_->drive({line[x], SyncFlag::BeamOn});
        }
    }
    video_out_->drive({0, SyncFlag::FrameEnd});
}
