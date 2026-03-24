#include "chip/video/bombjack_video/bombjack_video.hpp"

using namespace bombjack_video_constants;

void BombJackVideo::render_frame() {
    if (!tilemap_ || !attr_map_ || !char_rom_ || !video_out_) return;

    std::memset(pixel_buf_, 0, sizeof(pixel_buf_));

    // Render 32×28 visible foreground tiles
    // 3bpp planar: 24 bytes/tile (3 planes × 8 rows), plane_stride=8
    for (int ty = 0; ty < TILE_ROWS; ty++) {
        for (int tx = 0; tx < TILE_COLS; tx++) {
            int offs = ty * TILE_COLS + tx;
            uint8_t tile_idx = tilemap_[offs];
            uint8_t attr = attr_map_[offs];
            uint8_t pal_group = attr & 0x0F;
            bool flip_x = (attr & 0x40) != 0;
            bool flip_y = (attr & 0x80) != 0;

            if (tile_idx >= char_count_ && char_count_ > 0) tile_idx = 0;

            const uint8_t* tile = char_rom_ + tile_idx * BYTES_PER_TILE;
            uint8_t* dst = pixel_buf_ + ty * TILE_SIZE * WIDTH + tx * TILE_SIZE;

            tile_decoder::decode_planar_tile(
                tile, 1, 8, BPP, TILE_SIZE, TILE_SIZE,
                dst, WIDTH,
                static_cast<uint8_t>(pal_group * 8),
                flip_x, flip_y, 0x7F);
        }
    }

    drive_video_out();
}

void BombJackVideo::drive_video_out() {
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
