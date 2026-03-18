/*
 * tms9918_render.inc.hpp — Scanline rendering for TMS9918 VDP family
 *
 * Included from tms9918.hpp inside the tms9918 namespace.
 * Implements all four base screen modes (Graphics I, Text, Graphics II,
 * Multicolor) with if-constexpr gates for Sega and V9938 extended modes.
 */

// ============================================================================
// SCANLINE DISPATCH
// ============================================================================

template <const VDPTraits& Traits>
void tms9918_t<Traits>::render_scanline(int line) {
    // Fill with backdrop first
    std::memset(color_line_, backdrop_color(), 256);

    const uint8_t mode = current_screen_mode();

    switch (mode) {
    case ScreenMode::GRAPHIC_I:
        render_mode0_line(line);
        break;
    case ScreenMode::TEXT:
        render_mode1_line(line);
        break;
    case ScreenMode::GRAPHIC_II:
        render_mode2_line(line);
        break;
    case ScreenMode::MULTICOLOR:
        render_mode3_line(line);
        break;
    default:
        // Extended modes: Sega mode 4 or V9938 bitmap modes
        // (Placeholder — these will be filled in Phase 4/5)
        break;
    }

    // Sprites overlay the background in all graphic modes (not Text)
    if (mode != ScreenMode::TEXT) {
        render_sprites(line);
    }

    flush_scanline(line);
}

// ============================================================================
// MODE 0 — GRAPHICS I (256×192)
// ============================================================================
// 32×24 tiles from 8×8 pattern table.
// Name table: 768 bytes (32×24 tile indices).
// Pattern table: 2048 bytes (256 × 8 rows).
// Color table: 32 bytes — each byte covers 8 consecutive patterns
//   (upper nibble = foreground, lower nibble = background).

template <const VDPTraits& Traits>
void tms9918_t<Traits>::render_mode0_line(int line) {
    const uint16_t nt_base  = name_table_addr();
    const uint16_t ct_base  = color_table_addr();
    const uint16_t pg_base  = pattern_gen_addr();

    const int row = line >> 3;          // Tile row (0–23)
    const int fine_y = line & 0x07;     // Row within tile (0–7)

    for (int col = 0; col < 32; ++col) {
        // Look up tile index from name table
        const uint8_t tile = vram_[(nt_base + row * 32 + col) & Traits.vram_mask()];

        // Fetch pattern byte (1 bit per pixel, 8 pixels)
        const uint8_t pattern = vram_[(pg_base + tile * 8 + fine_y) & Traits.vram_mask()];

        // Fetch color: one byte covers 8 patterns (tile / 8)
        const uint8_t color = vram_[(ct_base + (tile >> 3)) & Traits.vram_mask()];
        const uint8_t fg = (color >> 4) & 0x0F;
        const uint8_t bg = color & 0x0F;

        // Render 8 pixels
        const int x = col * 8;
        for (int bit = 0; bit < 8; ++bit) {
            color_line_[x + bit] = (pattern & (0x80 >> bit)) ? fg : bg;
        }
    }
}

// ============================================================================
// MODE 1 — TEXT (240×192)
// ============================================================================
// 40×24 characters from 6×8 pattern table.
// Name table: 960 bytes (40×24 character indices).
// Pattern table: 2048 bytes (256 × 8 rows, only 6 bits used per row).
// No sprites in text mode. Colors from R7 (text/backdrop).
// 8-pixel left/right borders.

template <const VDPTraits& Traits>
void tms9918_t<Traits>::render_mode1_line(int line) {
    const uint16_t nt_base = name_table_addr();
    const uint16_t pg_base = pattern_gen_addr();

    const uint8_t fg = text_color();
    const uint8_t bg = backdrop_color();

    const int row = line >> 3;
    const int fine_y = line & 0x07;

    // 8-pixel left border (backdrop)
    // color_line_ is already filled with backdrop from render_scanline()

    for (int col = 0; col < 40; ++col) {
        const uint8_t ch = vram_[(nt_base + row * 40 + col) & Traits.vram_mask()];
        const uint8_t pattern = vram_[(pg_base + ch * 8 + fine_y) & Traits.vram_mask()];

        // 6 pixels per character, bit 7 is leftmost
        const int x = 8 + col * 6;  // 8-pixel left border offset
        for (int bit = 0; bit < 6; ++bit) {
            if (x + bit < 256) {
                color_line_[x + bit] = (pattern & (0x80 >> bit)) ? fg : bg;
            }
        }
    }
}

// ============================================================================
// MODE 2 — GRAPHICS II (256×192)
// ============================================================================
// 32×24 tiles, but pattern and color tables are divided into 3 regions
// of 256 patterns each (top/middle/bottom third of screen).
// Each pattern row has its own foreground/background color.
//
// Name table: 768 bytes.
// Pattern table: up to 6144 bytes (3 × 256 × 8).
// Color table: up to 6144 bytes (3 × 256 × 8, one byte per pattern row).
//
// R3 and R4 mask the table addresses:
//   Color table:   (R3 & 0x80) ? base | 0x1FFF : base
//   Pattern table: (R4 & 0x04) ? base | 0x1FFF : base

template <const VDPTraits& Traits>
void tms9918_t<Traits>::render_mode2_line(int line) {
    const uint16_t nt_base = name_table_addr();
    const uint16_t ct_base = static_cast<uint16_t>((regs_[reg::R3] & 0x80) << 6);
    const uint16_t pg_base = static_cast<uint16_t>((regs_[reg::R4] & 0x04) << 11);

    const uint16_t ct_mask = static_cast<uint16_t>((regs_[reg::R3] & 0x7F) << 3 | 0x07);
    const uint16_t pg_mask = static_cast<uint16_t>((regs_[reg::R4] & 0x03) << 8 | 0xFF);

    const int row = line >> 3;
    const int fine_y = line & 0x07;

    // Third of screen (0, 1, or 2) — each has its own 256-pattern region
    const int third = row / 8;      // 0=top (rows 0-7), 1=mid (8-15), 2=bot (16-23)
    const uint16_t region_offset = static_cast<uint16_t>(third * 256 * 8);

    for (int col = 0; col < 32; ++col) {
        const uint8_t tile = vram_[(nt_base + row * 32 + col) & Traits.vram_mask()];

        const uint16_t pattern_idx = static_cast<uint16_t>(tile * 8 + fine_y);

        // Apply mask: pattern/color lookup within region
        const uint8_t pattern = vram_[
            (pg_base + ((pattern_idx + region_offset) & (pg_mask << 3 | 0x07)))
            & Traits.vram_mask()];

        const uint8_t color = vram_[
            (ct_base + ((pattern_idx + region_offset) & (ct_mask << 3 | 0x07)))
            & Traits.vram_mask()];

        const uint8_t fg = (color >> 4) & 0x0F;
        const uint8_t bg = color & 0x0F;

        const int x = col * 8;
        for (int bit = 0; bit < 8; ++bit) {
            const uint8_t c = (pattern & (0x80 >> bit)) ? fg : bg;
            // Transparent (color 0) shows backdrop
            color_line_[x + bit] = (c == 0) ? backdrop_color() : c;
        }
    }
}

// ============================================================================
// MODE 3 — MULTICOLOR (64×48 blocks at 4×4 pixels each)
// ============================================================================
// Each name table entry points to a pattern; the pattern byte encodes
// two colors (upper nibble = left 4 pixels, lower nibble = right 4 pixels).
// The active row within the 8-byte pattern alternates per 4-pixel block row.

template <const VDPTraits& Traits>
void tms9918_t<Traits>::render_mode3_line(int line) {
    const uint16_t nt_base = name_table_addr();
    const uint16_t pg_base = pattern_gen_addr();

    const int row = line >> 3;
    const int fine_y = line & 0x07;

    // Two color rows per 8-pixel tile: rows 0–3 use offset (row%4)*2,
    // rows 4–7 use offset (row%4)*2+1
    const int color_row = (fine_y < 4) ? (fine_y >> 1) * 2 : (fine_y >> 1) * 2 + 1;
    // Simplified: each 4-line block maps to a pair of rows in the pattern
    // Actually in multicolor mode, the pattern offset is:
    //   (row & 3) * 2 + (fine_y >= 4 ? 1 : 0)
    // but each "row" in the pattern encodes a 4×4 block
    const int pattern_row = ((line >> 2) & 0x01)
                          ? ((row & 0x03) * 2 + 1)
                          : ((row & 0x03) * 2);

    for (int col = 0; col < 32; ++col) {
        const uint8_t tile = vram_[(nt_base + row * 32 + col) & Traits.vram_mask()];
        const uint8_t color_byte = vram_[
            (pg_base + tile * 8 + pattern_row) & Traits.vram_mask()];

        const uint8_t left_color  = (color_byte >> 4) & 0x0F;
        const uint8_t right_color = color_byte & 0x0F;

        const int x = col * 8;
        // Left 4 pixels
        for (int i = 0; i < 4; ++i) {
            color_line_[x + i] = (left_color == 0) ? backdrop_color() : left_color;
        }
        // Right 4 pixels
        for (int i = 4; i < 8; ++i) {
            color_line_[x + i] = (right_color == 0) ? backdrop_color() : right_color;
        }
    }
}
