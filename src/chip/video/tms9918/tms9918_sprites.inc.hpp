/*
 * tms9918_sprites.inc.hpp — Sprite rendering for TMS9918 VDP family
 *
 * Included from tms9918.hpp inside the tms9918 namespace.
 * Implements the original TMS9918 sprite model (32 sprites, 4 per line,
 * 8×8 or 16×16 with optional 2× magnification).
 *
 * Sega and V9938 sprite models are gated by if constexpr and will be
 * expanded in Phase 4/5.
 */

// ============================================================================
// SPRITE RENDERING
// ============================================================================
//
// Sprite attribute table (SAT) format — 4 bytes per sprite, 32 sprites:
//   Byte 0: Y position (vertical, 0-based; value 0xD0 terminates list)
//   Byte 1: X position (horizontal)
//   Byte 2: Pattern number (index into sprite pattern generator table)
//   Byte 3: [EC:1][0:3][Color:4]
//            EC = Early Clock (shift sprite 32 pixels left)
//            Color = sprite color (0 = transparent)
//
// Sprite pattern generator: 8 bytes per 8×8 pattern; 32 bytes per 16×16.
// 16×16 sprites use 4 consecutive 8×8 patterns arranged as:
//   [top-left][bottom-left][top-right][bottom-right]

template <const VDPTraits& Traits>
void tms9918_t<Traits>::render_sprites(int line) {
    if constexpr (Traits.sprite_model == VDPSpriteModel::ORIGINAL) {
        // === ORIGINAL TMS9918 SPRITE MODEL ===

        const uint16_t sat_base = sprite_attr_addr();
        const uint16_t spg_base = sprite_pattern_addr();
        const bool large = sprite_16x16();
        const bool mag = sprite_magnified();

        const int sprite_h = large ? 16 : 8;
        const int render_h = mag ? sprite_h * 2 : sprite_h;

        int sprites_on_line = 0;
        uint8_t fifth_sprite_num = 0;

        for (int s = 0; s < 32; ++s) {
            const uint16_t sat_addr = static_cast<uint16_t>(sat_base + s * 4);

            // Read Y position; 0xD0 terminates sprite list
            int sy = vram_[sat_addr & Traits.vram_mask()];
            if (sy == 0xD0) {
                // Set 5th sprite number to the next unprocessed sprite
                fifth_sprite_num = static_cast<uint8_t>(s);
                break;
            }

            // Y wraps: if Y >= 0xE0, treat as negative (Y - 256 + 1)
            sy = (sy + 1) & 0xFF;
            if (sy > 0xE0) sy -= 256;

            // Check if this sprite intersects the current scanline
            if (line < sy || line >= sy + render_h) continue;

            sprites_on_line++;
            if (sprites_on_line > 4) {
                // 5th sprite flag and number
                status_ |= reg::STATUS_5S;
                status_ = (status_ & ~reg::STATUS_5NUM)
                        | (static_cast<uint8_t>(s) & reg::STATUS_5NUM);
                break;  // Only 4 sprites per line
            }

            // Read remaining sprite attributes
            const int sx_raw = vram_[(sat_addr + 1) & Traits.vram_mask()];
            const uint8_t pattern_num = vram_[(sat_addr + 2) & Traits.vram_mask()];
            const uint8_t attr = vram_[(sat_addr + 3) & Traits.vram_mask()];

            const uint8_t color = attr & 0x0F;
            const bool early_clock = (attr & 0x80) != 0;

            // Transparent sprites are not rendered but still count
            if (color == 0) continue;

            int sx = sx_raw;
            if (early_clock) sx -= 32;

            // Row within sprite
            int sprite_row = line - sy;
            if (mag) sprite_row >>= 1;  // Magnified: each row displayed twice

            // For 16×16 sprites, pattern_num has bit 0 and bit 1 masked
            uint8_t pat = pattern_num;
            if (large) pat &= 0xFC;

            // Render 8 or 16 pixels wide
            const int sprite_w = large ? 16 : 8;
            const int render_w = mag ? sprite_w * 2 : sprite_w;

            for (int px = 0; px < render_w; ++px) {
                const int screen_x = sx + px;
                if (screen_x < 0 || screen_x >= 256) continue;

                // Determine which pixel in the sprite pattern
                int pat_x = mag ? (px >> 1) : px;
                int pat_y = sprite_row;

                // For 16×16, decode which of the 4 quadrant patterns
                uint8_t quad_pat = pat;
                if (large) {
                    // Layout: top-left (pat), bottom-left (pat+1),
                    //         top-right (pat+2), bottom-right (pat+3)
                    if (pat_x >= 8) { quad_pat += 2; pat_x -= 8; }
                    if (pat_y >= 8) { quad_pat += 1; pat_y -= 8; }
                }

                // Fetch pattern byte
                const uint8_t pattern_byte = vram_[
                    (spg_base + quad_pat * 8 + pat_y) & Traits.vram_mask()];

                // Test bit (MSB = leftmost pixel)
                if (pattern_byte & (0x80 >> pat_x)) {
                    // Collision detection: if pixel already set by a sprite
                    // (We use a simple approach: check if this pixel was
                    //  already written by a previous sprite on this line)
                    // For now, just write the color
                    color_line_[screen_x] = color;
                }
            }
        }

        // If fewer than 5 sprites found and no 5S flag, store last sprite checked
        if (sprites_on_line <= 4 && !(status_ & reg::STATUS_5S)) {
            // 5th sprite number field = number of sprites processed (or 31)
            status_ = (status_ & ~reg::STATUS_5NUM) | (fifth_sprite_num & reg::STATUS_5NUM);
        }

    } else if constexpr (Traits.sprite_model == VDPSpriteModel::SEGA) {
        // === SEGA SPRITE MODEL (Phase 4) ===
        // Placeholder: SMS sprites with 64 entries, 8 per line, etc.
        (void)line;

    } else if constexpr (Traits.sprite_model == VDPSpriteModel::V9938) {
        // === V9938 SPRITE MODEL (Phase 5) ===
        // Placeholder: sprite mode 2 with per-line color, CC/IC bits, etc.
        (void)line;
    }
}
