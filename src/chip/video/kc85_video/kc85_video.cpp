#include "chip/video/kc85_video/kc85_video.hpp"

using namespace kc85_video_constants;

// ============================================================================
// KC85/2,3: ZX Spectrum-like interleaved addressing
// ============================================================================
// IRM layout:
//   $0000-$1FFF: Left pixel area (256×256 pixels, 32 byte-columns)
//   $2000-$27FF: Right pixel area (64×256 pixels, 8 byte-columns)
//   $2800-$2FFF: Left color area (32×64 attr cells, 8×4 pixel groups)
//   $3000-$37FF: Right color area (8×64 attr cells)
//
// Pixel addressing (left, columns 0..31):
//   offset = col | ((y>>2 & 0x3) << 5) | ((y & 0x3) << 7) | ((y>>4 & 0xF) << 9)
// Color addressing (left):
//   offset = col | ((y>>2 & 0x3F) << 5)
//
// Pixel addressing (right, columns 32..39):
//   offset = 0x2000 + ((x&7) | ((y>>4 & 3)<<3) | ((y>>2 & 3)<<5) | ((y&3)<<7) | ((y>>6 & 3)<<9))
// Color addressing (right):
//   offset = 0x0800 + ((x&7) | ((y>>4 & 3)<<3) | ((y>>2 & 3)<<5) | ((y>>6 & 3)<<7))

void KC85VideoGenerator::render_standard() {
    const uint8_t* irm = irm_;
    for (int y = 0; y < HEIGHT; y++) {
        uint8_t* row = pixel_buf_ + y * WIDTH;

        // Left 256×256 area (columns 0..31)
        for (int x = 0; x < 32; x++) {
            int pixel_offset = x | (((y >> 2) & 0x3) << 5)
                                 | ((y & 0x3) << 7)
                                 | (((y >> 4) & 0xF) << 9);
            int color_offset = x | (((y >> 2) & 0x3F) << 5);
            uint8_t pixels = irm[pixel_offset];
            uint8_t color  = irm[0x2800 + color_offset];
            uint8_t bg = (color & 0x07) + BG_COLOR_OFFSET;
            uint8_t fg = (blink_bg_ && (color & 0x80)) ? bg : ((color >> 3) & 0x0F);
            int px = x * 8;
            for (int bit = 7; bit >= 0; --bit) {
                row[px++] = (pixels & (1 << bit)) ? fg : bg;
            }
        }

        // Right 64×256 area (columns 32..39)
        for (int x = 32; x < 40; x++) {
            int pixel_offset = 0x2000 + ((x & 0x7)
                                 | (((y >> 4) & 0x3) << 3)
                                 | (((y >> 2) & 0x3) << 5)
                                 | ((y & 0x3) << 7)
                                 | (((y >> 6) & 0x3) << 9));
            int color_offset = 0x0800 + ((x & 0x7)
                                 | (((y >> 4) & 0x3) << 3)
                                 | (((y >> 2) & 0x3) << 5)
                                 | (((y >> 6) & 0x3) << 7));
            uint8_t pixels = irm[pixel_offset];
            uint8_t color  = irm[0x2800 + color_offset];
            uint8_t bg = (color & 0x07) + BG_COLOR_OFFSET;
            uint8_t fg = (blink_bg_ && (color & 0x80)) ? bg : ((color >> 3) & 0x0F);
            int px = x * 8;
            for (int bit = 7; bit >= 0; --bit) {
                row[px++] = (pixels & (1 << bit)) ? fg : bg;
            }
        }
    }
}

// ============================================================================
// KC85/4: dual-plane, column-major layout with per-byte color
// ============================================================================
// IRM layout: 4 × 16 KB banks
//   Bank 0: pixel plane 0, Bank 1: color plane 0
//   Bank 2: pixel plane 1, Bank 3: color plane 1
//   Pixel data: bank_base[col * 256 + row]  (col = 0..39)
//   Color data: same layout in next bank

void KC85VideoGenerator::render_extended() {
    const uint8_t* pixel_base = irm_ + active_plane_ * 2 * 16384;
    const uint8_t* color_base = pixel_base + 16384;

    for (int y = 0; y < HEIGHT; y++) {
        uint8_t* row = pixel_buf_ + y * WIDTH;
        for (int col = 0; col < KC4_PIXEL_COLS; col++) {
            uint8_t pixels = pixel_base[col * 256 + y];
            uint8_t color  = color_base[col * 256 + y];
            uint8_t bg = (color & 0x07) + BG_COLOR_OFFSET;
            uint8_t fg = (blink_bg_ && (color & 0x80)) ? bg : ((color >> 3) & 0x0F);
            int x = col * 8;
            for (int bit = 7; bit >= 0; --bit) {
                row[x++] = (pixels & (1 << bit)) ? fg : bg;
            }
        }
    }
}

// ============================================================================
// Stream driving — per-scanline burst from internal pixel buffer
// ============================================================================

void KC85VideoGenerator::drive_stream() {
    if (!video_stream_) return;
    for (int y = 0; y < HEIGHT; y++) {
        const uint8_t* line = pixel_buf_ + y * WIDTH;
        video_stream_->drive({0, VideoFlags::HSync});
        for (int x = 0; x < WIDTH; x++) {
            video_stream_->drive({line[x], VideoFlags::BeamOn});
        }
    }
    video_stream_->drive({0, VideoFlags::FrameEnd});
}

// ============================================================================
// render_frame — main entry point
// ============================================================================

void KC85VideoGenerator::render_frame() {
    if (!irm_ || !video_stream_) return;

    if (mode_ == KC85VideoMode::Extended) {
        render_extended();
    } else {
        render_standard();
    }
    drive_stream();
}
