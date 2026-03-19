#pragma once

// ============================================================================
// Character Display Generator — TTL character-ROM video output
// ============================================================================
//
// Models the discrete TTL video generation circuitry used by simple
// character-mapped computer displays: character ROM lookup, optional
// color attribute RAM, bitmap expansion to pixels, and composite video
// signal output.
//
// Used by:
//   Z9001 / KC 87:  40×24 chars, 320×192 px, optional color RAM
//   Z1013:          32×32 chars, 256×256 px, monochrome
//
// Any character-grid system with 8-pixel-wide character ROMs and
// optional per-character color attributes can use this type.
//
// The generator does NOT own the VRAM, color RAM, or character ROM —
// it takes non-owning pointers updated by the system before each frame.
// ============================================================================

#include "core/signal/composite_video_stream.hpp"
#include "core/signal/video_flags.hpp"
#include "utils/charset_renderer.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

// Optional color attribute configuration
struct CharDisplayColorAttr {
    const uint8_t* color_ram  = nullptr;  // per-character color bytes (may be null)
    uint8_t fg_mask   = 0xFF;   // mask for foreground index
    int     fg_shift  = 0;      // right-shift after masking
    uint8_t bg_mask   = 0x00;   // mask for background index (0 = no per-char bg)
    int     bg_shift  = 0;      // right-shift after masking
};

struct CharDisplayGenerator {
    // --- Configuration (set once at init) ---
    void set_stream(CompositeVideoStream* s) { video_stream_ = s; }

    void set_geometry(int text_cols, int text_rows,
                      int char_width, int char_height,
                      int fb_width, int fb_height) {
        text_cols_   = text_cols;
        text_rows_   = text_rows;
        char_width_  = char_width;
        char_height_ = char_height;
        fb_width_    = fb_width;
        fb_height_   = fb_height;
        pixel_buf_.resize(fb_width * fb_height, 0);
    }

    void set_default_colors(uint8_t fg, uint8_t bg) {
        default_fg_ = fg;
        default_bg_ = bg;
    }

    // --- Per-frame dynamic state (set by system before render_frame) ---
    void set_vram(const uint8_t* v) { vram_ = v; }
    void set_char_rom(const uint8_t* r) { char_rom_ = r; }
    void set_color_attr(const CharDisplayColorAttr& a) { color_attr_ = a; }

    // --- Render one frame ---
    void render_frame();

private:
    void drive_stream();

    CompositeVideoStream* video_stream_ = nullptr;
    const uint8_t*        vram_         = nullptr;
    const uint8_t*        char_rom_     = nullptr;
    CharDisplayColorAttr  color_attr_   = {};

    int     text_cols_   = 40;
    int     text_rows_   = 24;
    int     char_width_  = 8;
    int     char_height_ = 8;
    int     fb_width_    = 320;
    int     fb_height_   = 192;
    uint8_t default_fg_  = 1;
    uint8_t default_bg_  = 0;
    std::vector<uint8_t> pixel_buf_;
};
