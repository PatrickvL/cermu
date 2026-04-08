#include "chip/video/bombjack_video/bombjack_video.hpp"

using namespace bombjack_video_constants;

// ============================================================================
// Frame rendering — paint all 3 layers into the indexed pixel buffer
// ============================================================================

void BombJackVideo::render_frame() {
    if (!video_out_) return;

    // Layer 1: Background (fills entire buffer)
    render_background();

    // Layer 2: Foreground tiles (transparent where pen == 0)
    render_foreground();

    // Layer 3: Hardware sprites (transparent where pen == 0)
    render_sprites();

    drive_video_out();
}

// ============================================================================
// Background layer — 16×16 tiles, 16×16 grid per image
// ============================================================================
//
// Map ROM layout: 8 images × 512 bytes (256 tile codes + 256 attributes).
// Each image selectable by register value (bits 2:0 = image, bit 4 = enable).
//
// Tile ROM: 3 planes × 8 KB each.  Each tile is 32 bytes per plane:
//   bytes  0–7:  top-left 8×8 sub-tile
//   bytes  8–15: top-right 8×8 sub-tile
//   bytes 16–23: bottom-left 8×8 sub-tile
//   bytes 24–31: bottom-right 8×8 sub-tile
//
// Each row within a 16-pixel-wide tile is assembled by combining
// the left sub-tile byte (shifted <<8) with the right sub-tile byte.
//
// Attribute bit 7 = horizontal flip.
// Lower 4 attribute bits = palette group (×8 colors = 7-bit index).

void BombJackVideo::render_background() {
    if (!bg_map_rom_ || !bg_tile_rom_[0] || !bg_tile_rom_[1] || !bg_tile_rom_[2]) {
        std::memset(pixel_buf_, 0, sizeof(pixel_buf_));
        return;
    }

    bool img_valid = (bg_image_select_ & 0x10) != 0;
    uint16_t img_base = (bg_image_select_ & 0x07) * 0x0200;

    for (int ty = 0; ty < 16; ty++) {
        for (int tx = 0; tx < 16; tx++) {
            int addr = img_base + ty * 16 + tx;
            uint8_t tile_code = img_valid ? bg_map_rom_[addr] : 0;
            uint8_t attr = bg_map_rom_[addr + 0x100];
            uint8_t color_block = (attr & 0x0F) << 3;
            bool flip_y = (attr & 0x80) != 0;

            int tile_y0 = ty * 16;
            int off = tile_code * 32;

            for (int yy = 0; yy < 16; yy++) {
                int src_yy = flip_y ? (15 - yy) : yy;
                int screen_y = tile_y0 + src_yy;

                // Gather 16 pixels from two 8-pixel halves
                int rom_row = yy;
                int rom_off = off + (rom_row < 8 ? rom_row : rom_row + 8);
                uint16_t bm0 = (uint16_t(bg_tile_rom_[0][rom_off]) << 8) | bg_tile_rom_[0][rom_off + 8];
                uint16_t bm1 = (uint16_t(bg_tile_rom_[1][rom_off]) << 8) | bg_tile_rom_[1][rom_off + 8];
                uint16_t bm2 = (uint16_t(bg_tile_rom_[2][rom_off]) << 8) | bg_tile_rom_[2][rom_off + 8];

                if (screen_y >= HEIGHT) continue;  // clip to visible area

                uint8_t* dst = pixel_buf_ + screen_y * WIDTH + tx * 16;
                for (int xx = 15; xx >= 0; xx--) {
                    uint8_t pen = ((bm2 >> xx) & 1) |
                                  (((bm1 >> xx) & 1) << 1) |
                                  (((bm0 >> xx) & 1) << 2);
                    *dst++ = color_block | pen;
                }
            }
        }
    }
}

// ============================================================================
// Foreground layer — 8×8 tiles, 32×28 visible (of 32×32)
// ============================================================================
//
// Character ROM: 3 planes concatenated. Each plane has 4 KB (512 tiles × 8 bytes).
// Plane offsets: plane 0 at char_rom_[0], plane 1 at char_rom_[4096],
//                plane 2 at char_rom_[8192].
//
// Tile code: 9 bits — 8 from tilemap byte, bit 8 from attr bit 4.
//   Total 512 tiles.
//
// Attribute byte:
//   bits [3:0] = palette group (×8 = 7-bit palette offset)
//   bit 4 = tile code bit 8
//   bit 6 = X flip
//   bit 7 = Y flip
//
// Transparency: if all 3 plane bits are 0 (pen == 0), pixel is transparent
// and the background shows through.

void BombJackVideo::render_foreground() {
    if (!tilemap_ || !attr_map_ || !char_rom_) return;

    // Char ROM has 3 planes concatenated: each plane = char_rom_size_ / 3 bytes
    int plane_size = char_rom_size_ / 3;

    for (int ty = 0; ty < TILE_ROWS; ty++) {
        for (int tx = 0; tx < TILE_COLS; tx++) {
            int offs = ty * TILE_COLS + tx;
            uint8_t chr = tilemap_[offs];
            uint8_t clr = attr_map_[offs];

            // 512 tile codes: 8 bits from tile RAM + bit 4 of color as 9th bit
            int tile_code = chr | ((clr & 0x10) << 4);
            uint8_t color_block = (clr & 0x0F) << 3;
            bool flip_x = (clr & 0x40) != 0;
            bool flip_y = (clr & 0x80) != 0;

            // Each 8×8 FG tile = 8 bytes per plane
            int tile_off = tile_code * 8;
            uint8_t* dst = pixel_buf_ + ty * TILE_SIZE * WIDTH + tx * TILE_SIZE;

            for (int py = 0; py < 8; py++) {
                int src_y = flip_y ? (7 - py) : py;
                uint8_t bm0 = char_rom_[tile_off + src_y];
                uint8_t bm1 = char_rom_[plane_size + tile_off + src_y];
                uint8_t bm2 = char_rom_[plane_size * 2 + tile_off + src_y];

                for (int px = 0; px < 8; px++) {
                    int src_x = flip_x ? px : (7 - px);
                    uint8_t pen = ((bm2 >> src_x) & 1) |
                                  (((bm1 >> src_x) & 1) << 1) |
                                  (((bm0 >> src_x) & 1) << 2);
                    // Transparent if pen == 0
                    if (pen != 0) {
                        dst[px] = color_block | pen;
                    }
                }
                dst += WIDTH;
            }
        }
    }
}

// ============================================================================
// Sprite layer — 24 hardware sprites, 16×16 or 32×32
// ============================================================================
//
// Sprite RAM at $9820–$987F: 24 sprites × 4 bytes.
// In our RAMChip ($9800), sprites start at offset 0x20.
//
// Byte 0: bit 7 = large (32×32), bits 6:0 = sprite tile code
// Byte 1: bit 7 = X flip, bit 6 = Y flip, bits 3:0 = palette group
// Byte 2: X position
// Byte 3: Y position
//
// Sprite ROMs: 3 planes × 8 KB each, same sub-tile structure as BG tiles.
//   16×16 sprite = 32 bytes per plane (same as BG tile).
//   32×32 sprite = 128 bytes per plane.
//
// Sprite 0 has highest priority (rendered last). We iterate backwards
// so later sprites overwrite earlier ones.

void BombJackVideo::render_sprites() {
    if (!sprite_ram_ || !sprite_rom_[0] || !sprite_rom_[1] || !sprite_rom_[2])
        return;

    // Sprite 0 = highest priority, render from 23 down to 0
    for (int spr = 23; spr >= 0; spr--) {
        const uint8_t* s = sprite_ram_ + 0x20 + spr * 4;
        uint8_t b0 = s[0];
        uint8_t b1 = s[1];
        uint8_t b2 = s[2];  // X position
        uint8_t b3 = s[3];  // Y position
        uint8_t color_block = (b1 & 0x0F) << 3;
        uint8_t sprite_code = b0 & 0x7F;

        if (b0 & 0x80) {
            // ── 32×32 large sprite ──────────────────────────────────
            int px = b2;
            int py = 225 - b3;
            int off = sprite_code * 128;

            for (int y = 0; y < 32; y++) {
                int screen_y = py + y;
                if (screen_y < 0 || screen_y >= HEIGHT) {
                    off++;
                    if ((y & 7) == 7) off += 8;
                    if ((y & 15) == 15) off += 32;
                    continue;
                }

                // Gather 32 pixels from 4 × 8-pixel sub-tiles
                uint32_t bm0 = (uint32_t(sprite_rom_[0][off]) << 24) |
                               (uint32_t(sprite_rom_[0][off + 8]) << 16) |
                               (uint32_t(sprite_rom_[0][off + 32]) << 8) |
                               uint32_t(sprite_rom_[0][off + 40]);
                uint32_t bm1 = (uint32_t(sprite_rom_[1][off]) << 24) |
                               (uint32_t(sprite_rom_[1][off + 8]) << 16) |
                               (uint32_t(sprite_rom_[1][off + 32]) << 8) |
                               uint32_t(sprite_rom_[1][off + 40]);
                uint32_t bm2 = (uint32_t(sprite_rom_[2][off]) << 24) |
                               (uint32_t(sprite_rom_[2][off + 8]) << 16) |
                               (uint32_t(sprite_rom_[2][off + 32]) << 8) |
                               uint32_t(sprite_rom_[2][off + 40]);
                off++;
                if ((y & 7) == 7) off += 8;
                if ((y & 15) == 15) off += 32;

                uint8_t* row = pixel_buf_ + screen_y * WIDTH;
                for (int x = 31; x >= 0; x--) {
                    uint8_t pen = ((bm2 >> x) & 1) |
                                  (((bm1 >> x) & 1) << 1) |
                                  (((bm0 >> x) & 1) << 2);
                    if (pen != 0) {
                        int screen_x = (px + (31 - x)) & 0xFF;
                        if (screen_x < WIDTH)
                            row[screen_x] = color_block | pen;
                    }
                }
            }
        } else {
            // ── 16×16 small sprite ──────────────────────────────────
            int px = b2;
            int py = 241 - b3;
            bool flip_x = (b1 & 0x80) != 0;
            bool flip_y = (b1 & 0x40) != 0;
            int off = sprite_code * 32;

            for (int y = 0; y < 16; y++) {
                int render_y = flip_x ? (15 - y) : y;
                int screen_y = py + render_y;
                if (screen_y < 0 || screen_y >= HEIGHT) {
                    off++;
                    if (y == 7) off += 8;
                    continue;
                }

                uint16_t bm0 = (uint16_t(sprite_rom_[0][off]) << 8) | sprite_rom_[0][off + 8];
                uint16_t bm1 = (uint16_t(sprite_rom_[1][off]) << 8) | sprite_rom_[1][off + 8];
                uint16_t bm2 = (uint16_t(sprite_rom_[2][off]) << 8) | sprite_rom_[2][off + 8];
                off++;
                if (y == 7) off += 8;

                uint8_t* row = pixel_buf_ + screen_y * WIDTH;
                if (flip_y) {
                    for (int x = 0; x <= 15; x++) {
                        uint8_t pen = ((bm2 >> x) & 1) |
                                      (((bm1 >> x) & 1) << 1) |
                                      (((bm0 >> x) & 1) << 2);
                        if (pen != 0) {
                            int screen_x = (px + x) & 0xFF;
                            if (screen_x < WIDTH)
                                row[screen_x] = color_block | pen;
                        }
                    }
                } else {
                    for (int x = 15; x >= 0; x--) {
                        uint8_t pen = ((bm2 >> x) & 1) |
                                      (((bm1 >> x) & 1) << 1) |
                                      (((bm0 >> x) & 1) << 2);
                        if (pen != 0) {
                            int screen_x = (px + (15 - x)) & 0xFF;
                            if (screen_x < WIDTH)
                                row[screen_x] = color_block | pen;
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// Video output — drive indexed pixel data through CompositeVideoOut
// ============================================================================

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
