#pragma once

#include <cstdint>
#include <cstring>

// ============================================================================
// CharsetRenderer — shared character-ROM glyph expansion
// ============================================================================
//
// Eliminates the identical font-lookup-and-pixel-expansion loop duplicated
// across Z1013, Z9001, MC6847, TextTerminal, MC6845, and others.
//
// All functions write palette indices (not RGBA) into a flat index buffer.
// The caller then flushes via VideoPixelUnit::flush_indexed_frame().
//
// Glyph format assumption: each font row is a byte with MSB = leftmost pixel.
// This is the universal convention for 8-pixel-wide character ROMs.
// ============================================================================

namespace charset_renderer {

/// Render a single character glyph into an index buffer.
///
/// @param dst          Pointer to top-left corner of glyph in index buffer
/// @param dst_stride   Index buffer row stride (pixels per row)
/// @param font_data    Pointer to first byte of this character in font ROM
/// @param char_height  Number of rows per character (typically 8, 10, 12)
/// @param char_width   Number of pixels per row (1..8, typically 8)
/// @param fg_idx       Foreground palette index (for set bits)
/// @param bg_idx       Background palette index (for clear bits)
inline void render_glyph(uint8_t* dst, int dst_stride,
                         const uint8_t* font_data, int char_height, int char_width,
                         uint8_t fg_idx, uint8_t bg_idx) {
    for (int gy = 0; gy < char_height; gy++) {
        uint8_t bits = font_data[gy];
        uint8_t* row = dst + gy * dst_stride;
        for (int gx = 0; gx < char_width; gx++) {
            row[gx] = (bits & (0x80u >> gx)) ? fg_idx : bg_idx;
        }
    }
}

/// Render a full character-grid screen into an index buffer.
///
/// Iterates over a text_cols × text_rows grid, looking up each screen
/// code in a font ROM and expanding it to pixels.
///
/// @param indices      Output index buffer (fb_width × fb_height)
/// @param fb_width     Framebuffer width in pixels
/// @param screen_ram   Screen codes (text_cols × text_rows bytes)
/// @param font_rom     Font ROM data (256 chars × char_height bytes)
/// @param text_cols    Number of character columns
/// @param text_rows    Number of character rows
/// @param char_width   Pixel width of each character (1..8)
/// @param char_height  Pixel height of each character
/// @param fg_idx       Foreground palette index
/// @param bg_idx       Background palette index
inline void render_screen(uint8_t* indices, int fb_width,
                          const uint8_t* screen_ram, const uint8_t* font_rom,
                          int text_cols, int text_rows,
                          int char_width, int char_height,
                          uint8_t fg_idx, uint8_t bg_idx) {
    for (int row = 0; row < text_rows; row++) {
        for (int col = 0; col < text_cols; col++) {
            uint8_t chr = screen_ram[row * text_cols + col];
            uint8_t* dst = indices + (row * char_height) * fb_width + (col * char_width);
            render_glyph(dst, fb_width,
                         font_rom + chr * char_height,
                         char_height, char_width, fg_idx, bg_idx);
        }
    }
}

/// Same as render_screen() but with per-character foreground color
/// from a color RAM array.
///
/// @param color_ram    Per-character color values (text_cols × text_rows bytes)
/// @param color_mask   Mask applied to each color_ram byte to extract fg index
/// @param bg_idx       Background palette index (same for all characters)
inline void render_screen_colored(uint8_t* indices, int fb_width,
                                  const uint8_t* screen_ram, const uint8_t* font_rom,
                                  const uint8_t* color_ram, uint8_t color_mask,
                                  int text_cols, int text_rows,
                                  int char_width, int char_height,
                                  uint8_t bg_idx) {
    for (int row = 0; row < text_rows; row++) {
        for (int col = 0; col < text_cols; col++) {
            uint8_t chr = screen_ram[row * text_cols + col];
            uint8_t fg  = color_ram[row * text_cols + col] & color_mask;
            uint8_t* dst = indices + (row * char_height) * fb_width + (col * char_width);
            render_glyph(dst, fb_width,
                         font_rom + chr * char_height,
                         char_height, char_width, fg, bg_idx);
        }
    }
}

/// Same as render_screen() but with per-character fg AND bg from color RAM.
///
/// @param color_ram    Per-character color attribute bytes
/// @param fg_mask      Mask for fg index (applied before shift)
/// @param fg_shift     Right-shift to apply after masking for fg
/// @param bg_mask      Mask for bg index (applied before shift)
/// @param bg_shift     Right-shift to apply after masking for bg
inline void render_screen_attr(uint8_t* indices, int fb_width,
                               const uint8_t* screen_ram, const uint8_t* font_rom,
                               const uint8_t* color_ram,
                               uint8_t fg_mask, int fg_shift,
                               uint8_t bg_mask, int bg_shift,
                               int text_cols, int text_rows,
                               int char_width, int char_height) {
    for (int row = 0; row < text_rows; row++) {
        for (int col = 0; col < text_cols; col++) {
            int pos = row * text_cols + col;
            uint8_t chr  = screen_ram[pos];
            uint8_t attr = color_ram[pos];
            uint8_t fg   = (attr & fg_mask) >> fg_shift;
            uint8_t bg   = (attr & bg_mask) >> bg_shift;
            uint8_t* dst = indices + (row * char_height) * fb_width + (col * char_width);
            render_glyph(dst, fb_width,
                         font_rom + chr * char_height,
                         char_height, char_width, fg, bg);
        }
    }
}

} // namespace charset_renderer
