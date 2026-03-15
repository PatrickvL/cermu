#pragma once

#include <cstdint>
#include <cstring>
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
// Two rendering modes:
//
//   CPU mode (default):
//     flush writes palette[color_line[x]] → framebuffer (RGBA).
//
//   GPU indexed mode (gpu_indexed = true):
//     flush copies raw palette indices → index_buffer.
//     The palette lookup is deferred to a GPU fragment shader,
//     eliminating per-pixel CPU work and reducing the texture upload
//     from 4 bytes/pixel (RGBA) to 1 byte/pixel (R8).
//
// Usage:
//   1. Chip allocates color_line (e.g. static array or malloc).
//   2. System calls set_framebuffer() with the RGBA output buffer.
//   3. (Optional) Host calls set_index_buffer() to enable GPU indexed mode.
//   4. During rendering, chip writes palette indices to color_line[x].
//   5. At scanline end, chip calls flush_indexed_line(row, palette, width).
//
// Chips that need additional per-line buffers (e.g. priority, collision)
// extend this struct via inheritance or composition.
// ============================================================================

struct VideoPixelUnit {
    uint8_t*  color_line  = nullptr;   // Per-pixel palette index buffer (chip-owned)
    uint32_t* framebuffer = nullptr;   // RGBA output framebuffer (system-owned)
    int       fb_width    = 0;         // Framebuffer width in pixels
    int       fb_height   = 0;         // Framebuffer height in pixels (raster lines)

    // GPU indexed rendering — when enabled, flush copies raw indices to
    // index_buffer instead of performing palette lookup into framebuffer.
    // The host allocates index_buffer and provides it via set_index_buffer().
    bool      gpu_indexed  = false;
    uint8_t*  index_buffer = nullptr;  // Full-frame index buffer (host-owned)

    // Set the destination framebuffer (CPU mode).
    inline void set_framebuffer(uint32_t* fb, int width, int height) {
        framebuffer = fb;
        fb_width    = width;
        fb_height   = height;
    }

    // Set the index buffer for GPU indexed mode.
    // Passing nullptr disables GPU indexed mode.
    inline void set_index_buffer(uint8_t* buf) {
        index_buffer = buf;
        gpu_indexed  = (buf != nullptr);
    }

    // Flush line_width indices from color_line into framebuffer row,
    // performing palette lookup.  The palette array must cover the full
    // index range stored in color_line (e.g. 16 for VIC-II, 128 for TED).
    inline void flush_indexed_line(int row, const uint32_t* palette, int line_width) const {
        flush_indexed_line_range(row, palette, 0, line_width);
    }

    // Flush a sub-range [x_start, x_end) of color_line into a framebuffer
    // row, performing palette lookup (CPU mode) or copying raw indices
    // (GPU indexed mode).  x_start/x_end are clamped to [0, fb_width).
    inline void flush_indexed_line_range(int row, const uint32_t* palette,
                                         int x_start, int x_end) const {
        if (!color_line) return;
        if (row < 0 || row >= fb_height) return;

        x_start = std::max(x_start, 0);
        x_end   = std::min(x_end, fb_width);

        if (gpu_indexed && index_buffer) {
            // GPU path — copy raw indices; palette applied by fragment shader.
            // memcpy is significantly cheaper than per-pixel LUT lookups.
            std::memcpy(index_buffer + row * fb_width + x_start,
                        color_line + x_start, x_end - x_start);
        } else if (framebuffer && palette) {
            // CPU path — resolve palette now.
            uint32_t* const row_ptr = framebuffer + row * fb_width;
            for (int x = x_start; x < x_end; ++x) {
                row_ptr[x] = palette[color_line[x]];
            }
        }
    }

    // ====================================================================
    // Per-frame flush — for systems that render the entire frame at once
    // (character-display machines, full-screen bitmap renderers) rather
    // than per-scanline.
    //
    // The caller fills an external frame_indices buffer (one byte per
    // pixel, fb_width × fb_height) with palette indices, then calls
    // this to route them:
    //   GPU mode → memcpy into index_buffer (palette applied by shader)
    //   CPU mode → palette lookup into framebuffer (RGBA)
    // ====================================================================

    inline void flush_indexed_frame(const uint8_t* frame_indices,
                                    const uint32_t* palette) const {
        const int total = fb_width * fb_height;
        if (total <= 0) return;

        if (gpu_indexed && index_buffer) {
            std::memcpy(index_buffer, frame_indices, total);
        } else if (framebuffer && palette) {
            for (int i = 0; i < total; ++i) {
                framebuffer[i] = palette[frame_indices[i]];
            }
        }
    }
};