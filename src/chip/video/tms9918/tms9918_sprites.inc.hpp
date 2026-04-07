/*
 * tms9918_sprites.inc.hpp — Sprite evaluation and compositing for TMS9918 VDP family
 *
 * Included from tms9918.hpp inside the tms9918 namespace.
 *
 * Per-dot-clock sprite pipeline:
 *   evaluate_sprites(line) — called during HBlank of the PREVIOUS line
 *     (dot 258). Scans the sprite attribute table, identifies sprites
 *     intersecting the target line, and pre-fetches their pattern data
 *     into sprite_buf_[]. Sets 5th-sprite and overflow flags.
 *
 *   composite_sprite_pixel(x, bg) — called once per active dot.
 *     Iterates the evaluated sprite buffer, tests each sprite's pattern
 *     bits at the current X position, applies priority (lower sprite
 *     number wins), and updates collision detection.
 *
 * Three sprite models (selected at compile time via VDPTraits):
 *   ORIGINAL — TMS9918/A: 32 sprites, 4/line, 8×8 or 16×16, 2× zoom
 *   SEGA     — SMS 315-5124: 64 sprites, 8/line, 8×8 or 8×16
 *   V9938    — Yamaha V9938: 32 sprites, 8/line, sprite mode 2 w/ per-line color
 */

// ============================================================================
// SPRITE EVALUATION — run during HBlank to prepare next line's sprites
// ============================================================================

template <const VDPTraits& Traits>
void tms9918_t<Traits>::evaluate_sprites(uint16_t line) {
    sprite_count_ = 0;

    if constexpr (Traits.sprite_model == VDPSpriteModel::ORIGINAL) {
        // === ORIGINAL TMS9918 SPRITE MODEL ===
        // 32 sprites in SAT, max 4 per scanline.
        // SAT entry: [Y, X, Pattern#, Attributes]

        const uint16_t sat_base = sprite_attr_addr();
        const uint16_t spg_base = sprite_pattern_addr();
        const bool large = sprite_16x16();
        const bool mag = sprite_magnified();

        const int sprite_h = large ? 16 : 8;
        const int render_h = mag ? sprite_h * 2 : sprite_h;
        const uint32_t mask = Traits.vram_mask();

        constexpr int max_per_line = 4;
        uint8_t last_sprite = 0;

        for (int s = 0; s < 32; ++s) {
            const uint16_t sat_addr = static_cast<uint16_t>(sat_base + s * 4);

            // Read Y position; 0xD0 terminates sprite list
            int sy = vram_[sat_addr & mask];
            if (sy == 0xD0) {
                last_sprite = static_cast<uint8_t>(s);
                break;
            }

            // Y wraps: Y position is (Y+1) on screen, values >= 0xE0 wrap negative
            sy = (sy + 1) & 0xFF;
            if (sy > 0xE0) sy -= 256;

            // Check if this sprite intersects the target scanline
            if (static_cast<int>(line) < sy || static_cast<int>(line) >= sy + render_h) {
                last_sprite = static_cast<uint8_t>(s + 1);
                continue;
            }

            if (sprite_count_ >= max_per_line) {
                // 5th sprite: set overflow flag and record its number
                status_ |= reg::STATUS_5S;
                status_ = (status_ & ~reg::STATUS_5NUM)
                        | (static_cast<uint8_t>(s) & reg::STATUS_5NUM);
                break;
            }

            // Read remaining attributes
            const int sx_raw = vram_[(sat_addr + 1) & mask];
            uint8_t pattern_num = vram_[(sat_addr + 2) & mask];
            const uint8_t attr = vram_[(sat_addr + 3) & mask];

            const uint8_t color = attr & 0x0F;
            const bool early_clock = (attr & 0x80) != 0;

            // Compute sprite row
            int sprite_row = static_cast<int>(line) - sy;
            if (mag) sprite_row >>= 1;  // Magnified: each row displayed twice

            // For 16×16 sprites, mask pattern_num bits 0-1
            if (large) pattern_num &= 0xFC;

            // Fetch pattern data
            SpriteEntry& entry = sprite_buf_[sprite_count_];
            entry.x = static_cast<int16_t>(early_clock ? sx_raw - 32 : sx_raw);
            entry.color = color;
            entry.width = large ? 16 : 8;
            entry.magnified = mag;

            // Fetch pattern bytes for this row
            if (large) {
                // 16×16: 4 quadrants — [TL, BL, TR, BR] each 8×8
                const int qy = sprite_row & 7;
                const int y_half = (sprite_row >= 8) ? 1 : 0;
                // Top-left / Bottom-left pattern
                entry.pattern[0] = vram_[(spg_base + (pattern_num + y_half) * 8 + qy) & mask];
                // Top-right / Bottom-right pattern
                entry.pattern[1] = vram_[(spg_base + (pattern_num + 2 + y_half) * 8 + qy) & mask];
            } else {
                // 8×8: single pattern byte
                entry.pattern[0] = vram_[(spg_base + pattern_num * 8 + sprite_row) & mask];
                entry.pattern[1] = 0;
            }

            sprite_count_++;
            last_sprite = static_cast<uint8_t>(s + 1);
        }

        // If no overflow, store the last sprite number processed
        if (!(status_ & reg::STATUS_5S)) {
            status_ = (status_ & ~reg::STATUS_5NUM) | (last_sprite & reg::STATUS_5NUM);
        }

    } else if constexpr (Traits.sprite_model == VDPSpriteModel::SEGA) {
        // === SEGA SPRITE MODEL ===
        // 64 sprites, max 8 per line, 8×8 or 8×16
        // SAT layout differs from TMS9918: Y positions in first 64 bytes,
        // X/pattern data starting at offset 128.

        const uint16_t sat_base = sprite_attr_addr();
        const uint16_t spg_base = sprite_pattern_addr();
        const bool tall = sprite_16x16();  // 8×16 when SI bit set
        const uint32_t mask = Traits.vram_mask();

        const int sprite_h = tall ? 16 : 8;
        constexpr int max_per_line = 8;

        for (int s = 0; s < 64; ++s) {
            // Y positions are stored in bytes 0-63 of the SAT
            int sy = vram_[(sat_base + s) & mask];

            // Terminator: 0xD0 on 192-line modes
            if (sy == 0xD0) break;

            // Y is 1-based on SMS (displayed at Y+1)
            sy = (sy + 1) & 0xFF;
            if (sy > 0xE0) sy -= 256;

            if (static_cast<int>(line) < sy || static_cast<int>(line) >= sy + sprite_h) {
                continue;
            }

            if (sprite_count_ >= max_per_line) {
                // 9th sprite overflow
                status_ |= reg::STATUS_5S;
                break;
            }

            // X and pattern data at SAT + 128 + s*2
            const uint16_t xp_addr = static_cast<uint16_t>(sat_base + 128 + s * 2);
            const int sx = vram_[xp_addr & mask];
            uint8_t pattern_num = vram_[(xp_addr + 1) & mask];

            int sprite_row = static_cast<int>(line) - sy;

            // For 8×16, pattern_num bit 0 is ignored (pairs)
            if (tall) pattern_num &= 0xFE;

            SpriteEntry& entry = sprite_buf_[sprite_count_];
            entry.x = static_cast<int16_t>(sx);
            // EC bit is handled via R0 bit 3 on SMS (shift all sprites left 8)
            if (BF_GET(regs_[reg::R0], 3:3)) {
                entry.x -= 8;
            }
            entry.color = 0;  // SMS sprites use CRAM, color from pattern data
            entry.width = 8;
            entry.magnified = false;

            // Fetch pattern: 4 bitplanes per row, 4 bytes per row
            const uint16_t pat_addr = static_cast<uint16_t>(
                pattern_num * (tall ? 32 : 32) + sprite_row * 4);
            entry.pattern[0] = vram_[(spg_base + pat_addr) & mask];
            entry.pattern[1] = vram_[(spg_base + pat_addr + 1) & mask];
            entry.pattern[2] = vram_[(spg_base + pat_addr + 2) & mask];
            entry.pattern[3] = vram_[(spg_base + pat_addr + 3) & mask];

            sprite_count_++;
        }

    } else if constexpr (Traits.sprite_model == VDPSpriteModel::V9938) {
        // === V9938 SPRITE MODE 2 ===
        // 32 sprites, max 8 per line, 8×8 or 16×16
        // Per-line sprite color table (separate from SAT)
        // CC (color composition) and IC (collision inhibit) bits

        const uint16_t sat_base = sprite_attr_addr();
        const uint16_t spg_base = sprite_pattern_addr();
        const bool large = sprite_16x16();
        const bool mag = sprite_magnified();
        const uint32_t mask = Traits.vram_mask();

        const int sprite_h = large ? 16 : 8;
        const int render_h = mag ? sprite_h * 2 : sprite_h;
        constexpr int max_per_line = 8;

        for (int s = 0; s < 32; ++s) {
            const uint16_t sat_addr = static_cast<uint16_t>(sat_base + s * 4);
            int sy = vram_[sat_addr & mask];

            if (sy == 0xD8) break;  // V9938 uses 0xD8 as terminator in 212-line mode

            sy = (sy + 1) & 0xFF;
            if (sy > 0xE0) sy -= 256;

            if (static_cast<int>(line) < sy || static_cast<int>(line) >= sy + render_h) {
                continue;
            }

            if (sprite_count_ >= max_per_line) {
                status_ |= reg::STATUS_5S;
                status_ = (status_ & ~reg::STATUS_5NUM)
                        | (static_cast<uint8_t>(s) & reg::STATUS_5NUM);
                break;
            }

            const int sx_raw = vram_[(sat_addr + 1) & mask];
            uint8_t pattern_num = vram_[(sat_addr + 2) & mask];
            const uint8_t attr = vram_[(sat_addr + 3) & mask];

            // V9938 sprite color table: located 512 bytes before SAT
            // Each sprite has 16 bytes (one per line), giving per-line color + EC + CC + IC
            const uint16_t sct_base = static_cast<uint16_t>(sat_base - 512);
            int sprite_row = static_cast<int>(line) - sy;
            if (mag) sprite_row >>= 1;
            const uint8_t line_color = vram_[(sct_base + s * 16 + sprite_row) & mask];

            const uint8_t color = line_color & 0x0F;
            const bool early_clock = (attr & 0x80) != 0;

            if (large) pattern_num &= 0xFC;

            SpriteEntry& entry = sprite_buf_[sprite_count_];
            entry.x = static_cast<int16_t>(early_clock ? sx_raw - 32 : sx_raw);
            entry.color = color;
            entry.width = large ? 16 : 8;
            entry.magnified = mag;

            if (large) {
                const int qy = sprite_row & 7;
                const int y_half = (sprite_row >= 8) ? 1 : 0;
                entry.pattern[0] = vram_[(spg_base + (pattern_num + y_half) * 8 + qy) & mask];
                entry.pattern[1] = vram_[(spg_base + (pattern_num + 2 + y_half) * 8 + qy) & mask];
            } else {
                entry.pattern[0] = vram_[(spg_base + pattern_num * 8 + sprite_row) & mask];
                entry.pattern[1] = 0;
            }

            sprite_count_++;
        }
    }
}

// ============================================================================
// SPRITE PIXEL COMPOSITING — called per dot during active display
// ============================================================================
//
// Iterates the pre-evaluated sprite buffer, tests each sprite's pattern
// at the current pixel X, applies:
//   - Lower sprite number wins (first opaque sprite in buffer order)
//   - Collision detection (STATUS_C set if 2+ sprites overlap at same pixel)
//   - Transparent sprites (color 0) are invisible but count for collision

template <const VDPTraits& Traits>
uint8_t tms9918_t<Traits>::composite_sprite_pixel(uint16_t pixel_x, uint8_t bg_pixel) {
    if (sprite_count_ == 0) return bg_pixel;

    if constexpr (Traits.sprite_model == VDPSpriteModel::ORIGINAL
               || Traits.sprite_model == VDPSpriteModel::V9938) {
        uint8_t result = bg_pixel;
        bool result_set = false;
        uint8_t collision_count = 0;

        for (uint8_t i = 0; i < sprite_count_; ++i) {
            const SpriteEntry& spr = sprite_buf_[i];
            const int dx = static_cast<int>(pixel_x) - spr.x;

            // Compute effective width with magnification
            const int eff_width = spr.magnified ? spr.width * 2 : spr.width;
            if (dx < 0 || dx >= eff_width) continue;

            // Map display pixel to pattern pixel
            const int pat_px = spr.magnified ? (dx >> 1) : dx;

            // Get pattern bit
            bool opaque;
            if (spr.width == 16) {
                // 16px wide: pattern[0] = left 8px, pattern[1] = right 8px
                const int half = (pat_px >= 8) ? 1 : 0;
                const int bit = pat_px - half * 8;
                opaque = (spr.pattern[half] & (0x80 >> bit)) != 0;
            } else {
                // 8px wide: pattern[0]
                opaque = (spr.pattern[0] & (0x80 >> pat_px)) != 0;
            }

            if (!opaque) continue;

            collision_count++;

            // First opaque sprite wins (unless color 0 = transparent ink)
            if (!result_set && spr.color != 0) {
                result = spr.color;
                result_set = true;
            }
        }

        // Sprite-sprite collision: set STATUS_C if 2+ sprite pixels overlap
        if (collision_count >= 2) {
            status_ |= reg::STATUS_C;
        }

        return result;

    } else if constexpr (Traits.sprite_model == VDPSpriteModel::SEGA) {
        // Sega SMS sprite compositing: 4bpp sprite pixels from bitplane data
        // BG priority: when bg_priority_[pixel_x] is set and BG pixel is
        // non-transparent (not palette index 0), BG wins over sprites.
        const bool bg_has_priority = bg_priority_[pixel_x]
                                  && (bg_pixel & 0x0F) != 0;

        uint8_t result = bg_pixel;
        bool result_set = false;
        uint8_t collision_count = 0;

        for (uint8_t i = 0; i < sprite_count_; ++i) {
            const SpriteEntry& spr = sprite_buf_[i];
            const int dx = static_cast<int>(pixel_x) - spr.x;
            if (dx < 0 || dx >= 8) continue;

            // Extract 4-bit color from bitplanes
            const int bit = 7 - dx;
            const uint8_t color_idx = static_cast<uint8_t>(
                ((spr.pattern[0] >> bit) & 1) |
                (((spr.pattern[1] >> bit) & 1) << 1) |
                (((spr.pattern[2] >> bit) & 1) << 2) |
                (((spr.pattern[3] >> bit) & 1) << 3));

            if (color_idx == 0) continue;  // Transparent

            collision_count++;

            if (!result_set && !bg_has_priority) {
                // SMS sprites always use second palette (entries 16-31)
                result = color_idx + 16;
                result_set = true;
            }
        }

        if (collision_count >= 2) {
            status_ |= reg::STATUS_C;
        }

        return result;
    }

    return bg_pixel;
}
