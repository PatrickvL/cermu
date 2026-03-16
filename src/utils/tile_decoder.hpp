#pragma once

#include <cstdint>

// ============================================================================
// tile_decoder — shared tile/character ROM decoding for arcade & home systems
// ============================================================================
//
// Many arcade boards (and some home systems) store character/tile graphics
// in ROM using planar bit arrangements.  The decode logic is nearly
// identical across systems, differing only in:
//
//   - Bits per pixel (1, 2, 3, 4)
//   - Plane layout within the tile ROM (consecutive, interleaved, split)
//   - Tile dimensions (usually 8×8, sometimes 8×16 or 16×16)
//   - Flip transforms (per-tile horizontal/vertical flip from attributes)
//
// This header provides a set of inline decoders that eliminate the per-
// system tile decode loops while remaining zero-overhead (all inlined).
//
// All decoders write palette indices into a destination buffer at arbitrary
// stride, making them composable with any framebuffer layout.
// ============================================================================

namespace tile_decoder {

// ============================================================================
// Standard planar decode — N consecutive planes, MSB-first
// ============================================================================
//
// ROM layout per tile: plane 0 (H bytes), plane 1 (H bytes), ..., plane N-1
// Each byte encodes 8 pixels; bit 7 = leftmost pixel.
//
// Used by: Bomb Jack (3bpp), many Capcom/Konami boards, generic NxM planar.

/// Decode one row of a planar tile into a destination buffer.
///
/// @param planes       Pointer to the first plane's row byte
/// @param plane_stride Distance (bytes) between consecutive planes' row data
/// @param bpp          Bits per pixel (1-8, typically 2-4)
/// @param dst          Destination palette index buffer
/// @param pal_base     Added to each decoded pixel value (palette group offset)
/// @param flip_x       Mirror horizontally if true
/// @param width        Pixels per row (1-8, typically 8)
/// @param idx_mask     AND mask applied to final index (0xFF = no masking)
inline void decode_planar_row(const uint8_t* planes, int plane_stride,
                              int bpp, uint8_t* dst, uint8_t pal_base,
                              bool flip_x, int width = 8,
                              uint8_t idx_mask = 0xFF) {
    for (int px = 0; px < width; ++px) {
        int src_bit = flip_x ? px : (width - 1 - px);
        uint8_t pixel = 0;
        for (int p = 0; p < bpp; ++p) {
            pixel |= ((planes[p * plane_stride] >> src_bit) & 1) << p;
        }
        dst[px] = (pal_base + pixel) & idx_mask;
    }
}

/// Decode a full planar tile into a framebuffer region.
///
/// @param tile_data     Start of tile data in ROM
/// @param bytes_per_row Bytes per row per plane (typically 1 for 8-wide tiles)
/// @param plane_stride  Bytes between plane 0 row and plane 1 row
///                      (tile_height for consecutive planes)
/// @param bpp           Bits per pixel
/// @param tile_w        Tile width in pixels (1-8)
/// @param tile_h        Tile height in pixels
/// @param dst           Destination pointer (top-left of tile in framebuffer)
/// @param dst_stride    Destination row stride (framebuffer width)
/// @param pal_base      Palette group offset added to each pixel
/// @param flip_x        Horizontal flip
/// @param flip_y        Vertical flip
/// @param idx_mask      AND mask applied to final index (0xFF = no masking)
inline void decode_planar_tile(const uint8_t* tile_data,
                               int bytes_per_row, int plane_stride,
                               int bpp, int tile_w, int tile_h,
                               uint8_t* dst, int dst_stride,
                               uint8_t pal_base,
                               bool flip_x = false, bool flip_y = false,
                               uint8_t idx_mask = 0xFF) {
    for (int py = 0; py < tile_h; ++py) {
        int src_y = flip_y ? (tile_h - 1 - py) : py;
        const uint8_t* row = tile_data + src_y * bytes_per_row;
        decode_planar_row(row, plane_stride, bpp,
                          dst + py * dst_stride, pal_base, flip_x, tile_w,
                          idx_mask);
    }
}

// ============================================================================
// Namco interleaved 2bpp decode — split left/right halves
// ============================================================================
//
// ROM layout per tile (16 bytes for 8×8):
//   Bytes 0-7:  right half (x=4-7), each byte has plane0 in [3:0], plane1 in [7:4]
//   Bytes 8-15: left half (x=0-3), same nibble layout
//
// Used by: Namco Pac-Man, Pengo, Galaga, and similar Namco boards.

/// Decode one row of a Namco interleaved 2bpp tile.
///
/// @param right_byte  ROM byte for the right half (x=4-7) of this row
/// @param left_byte   ROM byte for the left half (x=0-3) of this row
/// @param dst         Destination buffer (8 pixels)
/// @param color_table Color table PROM: maps (attr * 4 + pixel_2bit) → palette idx
/// @param color_attr  Color attribute for this tile (typically 6 bits)
/// @param idx_mask    AND mask applied to the colortable output (e.g. 0x1F)
inline void decode_namco_row(uint8_t right_byte, uint8_t left_byte,
                             uint8_t* dst,
                             const uint8_t* color_table,
                             uint8_t color_attr, uint8_t idx_mask) {
    // Left half (x=0-3): bit positions [0]-[3] for plane0, [4]-[7] for plane1
    for (int tx = 0; tx < 4; ++tx) {
        uint8_t p0 = (left_byte >> tx) & 1;
        uint8_t p1 = (left_byte >> (tx + 4)) & 1;
        uint8_t pixel = p0 | (p1 << 1);
        dst[3 - tx] = color_table[color_attr * 4 + pixel] & idx_mask;
    }
    // Right half (x=4-7)
    for (int tx = 0; tx < 4; ++tx) {
        uint8_t p0 = (right_byte >> tx) & 1;
        uint8_t p1 = (right_byte >> (tx + 4)) & 1;
        uint8_t pixel = p0 | (p1 << 1);
        dst[4 + 3 - tx] = color_table[color_attr * 4 + pixel] & idx_mask;
    }
}

/// Decode a full Namco interleaved 2bpp 8×8 tile.
///
/// @param tile_data    Start of tile (16 bytes: 8 right-half + 8 left-half rows)
/// @param dst          Destination pointer (top-left of tile in framebuffer)
/// @param dst_stride   Framebuffer row stride
/// @param color_table  Color table PROM
/// @param color_attr   Color attribute for this tile
/// @param idx_mask     AND mask for palette index (e.g. 0x1F for 32-entry palette)
inline void decode_namco_tile(const uint8_t* tile_data,
                              uint8_t* dst, int dst_stride,
                              const uint8_t* color_table,
                              uint8_t color_attr, uint8_t idx_mask = 0x1F) {
    for (int ty = 0; ty < 8; ++ty) {
        decode_namco_row(tile_data[ty], tile_data[ty + 8],
                         dst + ty * dst_stride,
                         color_table, color_attr, idx_mask);
    }
}

} // namespace tile_decoder
