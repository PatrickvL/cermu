#pragma once

#include <cstdint>
#include <algorithm>

// ============================================================================
// VideoPixelUnit — shared scanline index buffer + deferred palette flush
// ============================================================================
//
// All video chips store palette indices into a per-scanline color_line
// buffer during rendering, then call flush_indexed_line() at the end of
// each visible scanline to resolve indices to ARGB via a palette lookup.
//
// The struct holds only pointers and dimensions — buffer allocation is
// chip-specific.  Systems provide the framebuffer via set_framebuffer().
//
// Usage:
//   1. Chip allocates color_line (e.g. static array or malloc).
//   2. System calls set_framebuffer() with the RGBA output buffer.
//   3. During rendering, chip writes palette indices to color_line[x].
//   4. At scanline end, chip calls flush_indexed_line(row, palette, width).
//
// Chips that need additional per-line buffers (e.g. priority, collision)
// extend this struct via inheritance or composition.
// ============================================================================

struct VideoPixelUnit {
    uint8_t*  color_line  = nullptr;   // Per-pixel palette index buffer (chip-owned)
    uint32_t* framebuffer = nullptr;   // RGBA output framebuffer (system-owned)
    int       fb_width    = 0;         // Framebuffer width in pixels
    int       fb_height   = 0;         // Framebuffer height in pixels (raster lines)

    // Set the destination framebuffer.
    inline void set_framebuffer(uint32_t* fb, int width, int height) {
        framebuffer = fb;
        fb_width    = width;
        fb_height   = height;
    }

    // Flush line_width indices from color_line into framebuffer row,
    // performing palette lookup.  The palette array must cover the full
    // index range stored in color_line (e.g. 16 for VIC-II, 128 for TED).
    inline void flush_indexed_line(int row, const uint32_t* palette, int line_width) const {
        flush_indexed_line_range(row, palette, 0, line_width);
    }

    // Flush a sub-range [x_start, x_end) of color_line into a framebuffer
    // row, performing palette lookup.  Useful when palette changes mid-
    // scanline and pixels already emitted must be flushed with the old LUT
    // before rebuilding.  x_start/x_end are clamped to [0, fb_width).
    inline void flush_indexed_line_range(int row, const uint32_t* palette,
                                         int x_start, int x_end) const {
        if (!framebuffer || !color_line || !palette) return;
        if (row < 0 || row >= fb_height) return;

        x_start = std::max(x_start, 0);
        x_end   = std::min(x_end, fb_width);

        uint32_t* const row_ptr = framebuffer + row * fb_width;
        for (int x = x_start; x < x_end; ++x) {
            row_ptr[x] = palette[color_line[x]];
        }
    }
};