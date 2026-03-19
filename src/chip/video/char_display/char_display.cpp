#include "chip/video/char_display/char_display.hpp"

void CharDisplayGenerator::render_frame() {
    if (!vram_ || !char_rom_ || !video_stream_) return;
    if (pixel_buf_.empty()) return;

    uint8_t* idx    = pixel_buf_.data();
    int      stride = fb_width_;

    std::memset(idx, 0, pixel_buf_.size());

    // Select rendering path based on color attribute availability
    if (color_attr_.color_ram && color_attr_.bg_mask) {
        // Per-character fg AND bg from color RAM
        charset_renderer::render_screen_attr(
            idx, stride,
            vram_, char_rom_, color_attr_.color_ram,
            color_attr_.fg_mask, color_attr_.fg_shift,
            color_attr_.bg_mask, color_attr_.bg_shift,
            text_cols_, text_rows_, char_width_, char_height_);
    } else if (color_attr_.color_ram) {
        // Per-character fg from color RAM, fixed bg
        charset_renderer::render_screen_colored(
            idx, stride,
            vram_, char_rom_, color_attr_.color_ram, color_attr_.fg_mask,
            text_cols_, text_rows_, char_width_, char_height_,
            default_bg_);
    } else {
        // Fixed fg/bg (monochrome)
        charset_renderer::render_screen(
            idx, stride,
            vram_, char_rom_,
            text_cols_, text_rows_, char_width_, char_height_,
            default_fg_, default_bg_);
    }

    drive_stream();
}

void CharDisplayGenerator::drive_stream() {
    if (!video_stream_) return;
    const uint8_t* idx = pixel_buf_.data();
    for (int y = 0; y < fb_height_; y++) {
        const uint8_t* line = idx + y * fb_width_;
        video_stream_->drive({0, VideoFlags::HSync});
        for (int x = 0; x < fb_width_; x++) {
            video_stream_->drive({line[x], VideoFlags::BeamOn});
        }
    }
    video_stream_->drive({0, VideoFlags::FrameEnd});
}
