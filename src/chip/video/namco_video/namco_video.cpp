#include "chip/video/namco_video/namco_video.hpp"

using namespace namco_video_constants;

// ============================================================================
// Already-rotated 224×288 raster output
// ============================================================================
//
// The real hardware generates a 288×224 raster with the monitor rotated
// 90° CW.  The TTL address generation maps VRAM entries such that the
// scanline-order output appears correct on the rotated display.
//
// This generator applies that same address mapping directly, producing
// a 224×288 (28 tile-cols × 36 tile-rows) output that is ready for
// on-screen display without GPU rotation.
//
// Screen tile (tx, ty) → VRAM offset mapping:
//   ty ∈ [0,1]:   top score strip   → VRAM row = ty+30, col = tx+2
//   ty ∈ [2,33]:  main playfield    → VRAM row = 29-tx, col = ty-2
//   ty ∈ [34,35]: bottom score strip → VRAM row = ty-34, col = tx+2
//
// TODO(blocked): output native 288×224 signal when GPU-side rotation is available.
//   Requires HardwareTraits rotation field + shader support.

void NamcoVideo::render_frame() {
    if (!vram_ || !cram_ || !char_rom_ || !colortable_ || !video_out_) return;

    std::memset(pixel_buf_, 0, sizeof(pixel_buf_));

    // Render all tiles in already-rotated order (28 columns × 36 rows)
    for (int ty = 0; ty < VISIBLE_TILE_ROWS; ty++) {
        for (int tx = 0; tx < VISIBLE_TILE_COLS; tx++) {
            int mx, my;
            if (ty < 2) {
                // Top score strip → VRAM rows 30-31
                mx = tx + 2;
                my = ty + 30;
            } else if (ty >= 34) {
                // Bottom score strip → VRAM rows 0-1
                mx = tx + 2;
                my = ty - 34;
            } else {
                // Main playfield → VRAM rows 2-29
                mx = ty - 2;
                my = 29 - tx;
            }

            if (mx < 0 || mx >= VRAM_COLS || my < 0 || my >= VRAM_COLS)
                continue;

            int offs = my * VRAM_COLS + mx;
            uint8_t tile_idx = vram_[offs];
            uint8_t color_attr = cram_[offs] & 0x3F;

            if (tile_idx >= char_count_) tile_idx = 0;

            const uint8_t* tile = char_rom_ + tile_idx * BYTES_PER_TILE;

            int fb_x = tx * TILE_SIZE;
            int fb_y = ty * TILE_SIZE;
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
