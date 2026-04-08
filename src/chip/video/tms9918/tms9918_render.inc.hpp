/*
 * tms9918_render.inc.hpp — Per-dot background rendering for TMS9918 VDP family
 *
 * Included from tms9918.hpp inside the tms9918 namespace.
 *
 * Per-dot-clock pipeline:
 *   begin_scanline()   — called at dot 0 of each visible line.
 *                        Caches per-line values, prefetches first tile.
 *   bg_fetch_step()    — called each dot during active display (dots 0-255).
 *                        Reads VRAM for the NEXT tile at specific sub-cycles.
 *   emit_bg_pixel()    — called each dot during active display.
 *                        Shifts out one pixel from the current tile data.
 *
 * The pipeline is 1 tile ahead: while emitting pixels from tile N,
 * VRAM reads for tile N+1 proceed in the background. The first tile's
 * data is prefetched during begin_scanline() so it's ready at dot 0.
 */

// ============================================================================
// BEGIN SCANLINE — per-line setup + first tile prefetch
// ============================================================================

template <const VDPTraits& Traits>
void tms9918_t<Traits>::begin_scanline() {
    const uint16_t line = line_;

    // Decode current screen mode from registers
    screen_mode_ = decode_screen_mode(regs_[reg::R0], regs_[reg::R1]);

    // Cache per-line values that don't change within a scanline
    bg_.row = static_cast<uint8_t>(line >> 3);
    bg_.tile_row = static_cast<uint8_t>(line & 0x07);
    bg_.nt_base = name_table_addr();
    bg_.pg_base = pattern_gen_addr();
    bg_.ct_base = color_table_addr();
    bg_.column = 0;
    bg_.pixel_in_char = 0;

    // Mode-specific setup
    switch (screen_mode_) {
    case ScreenMode::TEXT:
        bg_.char_width = 6;
        break;

    case ScreenMode::GRAPHIC_II:
        // Mode 2 address masking (R3/R4 control table mirroring)
        bg_.ct_base = static_cast<uint16_t>((regs_[reg::R3] & 0x80) << 6);
        bg_.pg_base = static_cast<uint16_t>((regs_[reg::R4] & 0x04) << 11);
        bg_.ct_mask = static_cast<uint16_t>((regs_[reg::R3] & 0x7F) << 3 | 0x07);
        bg_.pg_mask = static_cast<uint16_t>((regs_[reg::R4] & 0x03) << 8 | 0xFF);
        bg_.region_offset = static_cast<uint16_t>((bg_.row / 8) * 256 * 8);
        bg_.char_width = 8;
        break;

    default:  // GRAPHIC_I, MULTICOLOR, extended modes
        bg_.char_width = 8;
        break;
    }

    // Sega fine-scroll: start mid-tile so the first tile is partially
    // clipped on the left, producing sub-pixel horizontal scrolling.
    // R0.D6: inhibit horizontal scroll for the top 2 character rows (lines 0-15).
    if constexpr (Traits.has_scroll()) {
        if (screen_mode_ >= ScreenMode::SEGA_MODE4) {
            const bool h_inhibit = (regs_[reg::R0] & 0x40) && line_ < 16;
            bg_.pixel_in_char = h_inhibit ? 0 : (this->scroll_x_ & 7);
        }
    }

    // Clear collision tracking for this line
    std::memset(sprite_collision_, 0, 256);

    // Evaluate sprites for line 0 (subsequent lines evaluated during HBlank)
    if (line == 0) {
        evaluate_sprites(0);
    }

    // Prefetch first tile data so it's ready for dot 0
    prefetch_tile(0);

    // Load shift register from prefetch latches
    load_bg_shifter();
}

// ============================================================================
// PREFETCH TILE — read VRAM for a specific column (mode-aware)
// ============================================================================

template <const VDPTraits& Traits>
void tms9918_t<Traits>::prefetch_tile(uint8_t col) {
    const uint32_t mask = Traits.vram_mask();

    switch (screen_mode_) {
    case ScreenMode::GRAPHIC_I: {
        // Name table → tile index
        bg_.name_latch = vram_[(bg_.nt_base + bg_.row * 32 + col) & mask];
        // Pattern generator → 8-pixel bit pattern
        bg_.pattern_latch = vram_[(bg_.pg_base + bg_.name_latch * 8 + bg_.tile_row) & mask];
        // Color table → one byte covers 8 consecutive patterns (tile / 8)
        bg_.color_latch = vram_[(bg_.ct_base + (bg_.name_latch >> 3)) & mask];
        break;
    }
    case ScreenMode::TEXT: {
        // Name table → character index (40 columns)
        bg_.name_latch = vram_[(bg_.nt_base + bg_.row * 40 + col) & mask];
        // Pattern generator → 6-pixel bit pattern (upper 6 bits used)
        bg_.pattern_latch = vram_[(bg_.pg_base + bg_.name_latch * 8 + bg_.tile_row) & mask];
        // No color table — text/backdrop from R7
        bg_.color_latch = regs_[reg::R7];
        break;
    }
    case ScreenMode::GRAPHIC_II: {
        // Name table → tile index
        bg_.name_latch = vram_[(bg_.nt_base + bg_.row * 32 + col) & mask];
        const uint16_t pattern_idx = static_cast<uint16_t>(bg_.name_latch * 8 + bg_.tile_row);
        // Pattern and color with region offset + mask (3-zone address space)
        bg_.pattern_latch = vram_[
            (bg_.pg_base + ((pattern_idx + bg_.region_offset) & (bg_.pg_mask << 3 | 0x07)))
            & mask];
        bg_.color_latch = vram_[
            (bg_.ct_base + ((pattern_idx + bg_.region_offset) & (bg_.ct_mask << 3 | 0x07)))
            & mask];
        break;
    }
    case ScreenMode::MULTICOLOR: {
        // Name table → tile index
        bg_.name_latch = vram_[(bg_.nt_base + bg_.row * 32 + col) & mask];
        // Pattern byte encodes two 4-pixel color blocks
        // Row within pattern depends on line position (4-line groups)
        const uint8_t pattern_row = static_cast<uint8_t>(
            ((line_ >> 2) & 0x01)
            ? ((bg_.row & 0x03) * 2 + 1)
            : ((bg_.row & 0x03) * 2));
        bg_.pattern_latch = vram_[(bg_.pg_base + bg_.name_latch * 8 + pattern_row) & mask];
        // Decode left/right colors
        bg_.mc_left = (bg_.pattern_latch >> 4) & 0x0F;
        bg_.mc_right = bg_.pattern_latch & 0x0F;
        break;
    }
    default:
        // Extended modes (Sega mode 4, V9938 bitmap modes)
        if constexpr (Traits.is_sega()) {
            prefetch_tile_sega(col);
        }
        break;
    }
}

// ============================================================================
// LOAD BG SHIFTER — transfer latched data into shift register + colors
// ============================================================================

template <const VDPTraits& Traits>
void tms9918_t<Traits>::load_bg_shifter() {
    switch (screen_mode_) {
    case ScreenMode::GRAPHIC_I:
    case ScreenMode::GRAPHIC_II:
        bg_.shift_reg = bg_.pattern_latch;
        bg_.fg_color = (bg_.color_latch >> 4) & 0x0F;
        bg_.bg_color = bg_.color_latch & 0x0F;
        break;

    case ScreenMode::TEXT:
        bg_.shift_reg = bg_.pattern_latch;
        bg_.fg_color = text_color();
        bg_.bg_color = backdrop_color();
        break;

    case ScreenMode::MULTICOLOR:
        // No shift register — direct color blocks
        break;

    default:
        if constexpr (Traits.is_sega()) {
            load_bg_shifter_sega();
        }
        break;
    }
}

// ============================================================================
// BG FETCH STEP — per-dot VRAM fetch pipeline
// ============================================================================
//
// During active display (dots 0-255), this runs each dot.
// Pipelined: while emitting from tile N, we fetch tile N+1.
//
// For 8-pixel tiled modes (Graphic I/II, Multicolor):
//   sub-cycle 0: load SR from latches + begin name fetch for next tile
//   sub-cycle 2: fetch pattern for next tile
//   sub-cycle 4: fetch color/pattern for next tile
//   sub-cycle 6: (prepare for next load)
//
// For 6-pixel text mode:
//   sub-cycle 0: load SR from latches + begin name fetch for next char
//   sub-cycle 2: fetch pattern for next char
//   sub-cycle 4: (prepare for next load)

template <const VDPTraits& Traits>
void tms9918_t<Traits>::bg_fetch_step() {
    const uint8_t sub = bg_.pixel_in_char;
    const uint32_t mask = Traits.vram_mask();

    // Text mode: 8-pixel left border (dots 0-7), no tile fetch
    if (screen_mode_ == ScreenMode::TEXT && dot_ < 8) {
        return;
    }
    // Text mode: 8-pixel right border (dots 248-255), no tile fetch
    if (screen_mode_ == ScreenMode::TEXT && dot_ >= 248) {
        return;
    }

    // Next column to pre-fetch (pipeline is 1 tile ahead)
    const uint8_t next_col = bg_.column + 1;
    const uint8_t max_cols = (screen_mode_ == ScreenMode::TEXT) ? 40 : 32;

    switch (sub) {
    case 0:
        // Load shift register with previously-fetched data
        load_bg_shifter();

        if constexpr (Traits.is_sega()) {
            // Sega mode 4: full tile prefetch (nametable + 4 bitplanes)
            // for the current column.  Unlike TMS modes, there's no
            // pipelined fetch — everything loads in one shot.
            if (screen_mode_ >= ScreenMode::SEGA_MODE4) {
                prefetch_tile_sega(bg_.column);
                break;
            }
        }

        // Begin fetching next tile (if not past last column)
        if (next_col < max_cols) {
            // Name table read for next tile
            if (screen_mode_ == ScreenMode::TEXT) {
                bg_.name_latch = vram_[(bg_.nt_base + bg_.row * 40 + next_col) & mask];
            } else {
                bg_.name_latch = vram_[(bg_.nt_base + bg_.row * 32 + next_col) & mask];
            }
        }
        break;

    case 2:
        // Pattern fetch for next tile
        if (next_col < max_cols) {
            switch (screen_mode_) {
            case ScreenMode::GRAPHIC_I:
            case ScreenMode::TEXT:
                bg_.pattern_latch = vram_[(bg_.pg_base + bg_.name_latch * 8 + bg_.tile_row) & mask];
                break;
            case ScreenMode::GRAPHIC_II: {
                const uint16_t pattern_idx = static_cast<uint16_t>(bg_.name_latch * 8 + bg_.tile_row);
                bg_.pattern_latch = vram_[
                    (bg_.pg_base + ((pattern_idx + bg_.region_offset) & (bg_.pg_mask << 3 | 0x07)))
                    & mask];
                break;
            }
            case ScreenMode::MULTICOLOR: {
                const uint8_t pattern_row = static_cast<uint8_t>(
                    ((line_ >> 2) & 0x01)
                    ? ((bg_.row & 0x03) * 2 + 1)
                    : ((bg_.row & 0x03) * 2));
                bg_.pattern_latch = vram_[(bg_.pg_base + bg_.name_latch * 8 + pattern_row) & mask];
                bg_.mc_left = (bg_.pattern_latch >> 4) & 0x0F;
                bg_.mc_right = bg_.pattern_latch & 0x0F;
                break;
            }
            default:
                break;
            }
        }
        break;

    case 4:
        // Color fetch for next tile (modes that use color table)
        if (next_col < max_cols) {
            switch (screen_mode_) {
            case ScreenMode::GRAPHIC_I:
                bg_.color_latch = vram_[(bg_.ct_base + (bg_.name_latch >> 3)) & mask];
                break;
            case ScreenMode::GRAPHIC_II: {
                const uint16_t pattern_idx = static_cast<uint16_t>(bg_.name_latch * 8 + bg_.tile_row);
                bg_.color_latch = vram_[
                    (bg_.ct_base + ((pattern_idx + bg_.region_offset) & (bg_.ct_mask << 3 | 0x07)))
                    & mask];
                break;
            }
            case ScreenMode::TEXT:
                // Text mode: no color table, colors from R7 (already set)
                bg_.color_latch = regs_[reg::R7];
                break;
            default:
                break;
            }
        }
        break;

    default:
        break;
    }

    // Advance pixel-within-character counter
    bg_.pixel_in_char++;
    if (bg_.pixel_in_char >= bg_.char_width) {
        bg_.pixel_in_char = 0;
        bg_.column++;
    }
}

// ============================================================================
// EMIT BG PIXEL — extract one pixel from shift register or color block
// ============================================================================

template <const VDPTraits& Traits>
uint8_t tms9918_t<Traits>::emit_bg_pixel() {
    const uint8_t bd = backdrop_color();

    switch (screen_mode_) {
    case ScreenMode::GRAPHIC_I:
    case ScreenMode::GRAPHIC_II: {
        // 1 bit per pixel from shift register (MSB = leftmost)
        const uint8_t pixel = (bg_.shift_reg & 0x80) ? bg_.fg_color : bg_.bg_color;
        bg_.shift_reg <<= 1;
        // Transparent (color 0) shows backdrop
        return (pixel == 0) ? bd : pixel;
    }
    case ScreenMode::TEXT: {
        // Left/right border: backdrop
        if (dot_ < 8 || dot_ >= 248) {
            return bd;
        }
        // 6 bits per character (MSB = leftmost), bits 7-2
        const uint8_t pixel = (bg_.shift_reg & 0x80) ? bg_.fg_color : bg_.bg_color;
        bg_.shift_reg <<= 1;
        return pixel;
    }
    case ScreenMode::MULTICOLOR: {
        // 4 pixels left color, 4 pixels right color
        const uint8_t sub = bg_.pixel_in_char;
        // pixel_in_char has already been incremented by bg_fetch_step(),
        // so the value we see here is 1 ahead. Compensate:
        const uint8_t actual_sub = (sub == 0) ? (bg_.char_width - 1) : (sub - 1);
        const uint8_t color = (actual_sub < 4) ? bg_.mc_left : bg_.mc_right;
        return (color == 0) ? bd : color;
    }
    default:
        // Extended modes
        if constexpr (Traits.is_sega()) {
            return emit_bg_pixel_sega();
        }
        return bd;
    }
}

// ============================================================================
// SEGA MODE 4 — per-dot background rendering
// ============================================================================
// SMS Mode 4: 256×192 (or 224/240 in extended), 32×28 tile map,
// 4bpp tiles (8×8, 32 bytes each), 32-entry CRAM, per-line H-scroll,
// per-column V-scroll, tile priority bit.

template <const VDPTraits& Traits>
void tms9918_t<Traits>::prefetch_tile_sega([[maybe_unused]] uint8_t col) {
    if constexpr (Traits.is_sega()) {
        const uint32_t mask = Traits.vram_mask();

        // Sega mode 4 name table entry is 16 bits:
        //   bit  0-8:  pattern index (0-511)
        //   bit  9:    horizontal flip
        //   bit 10:    vertical flip
        //   bit 11:    palette select (0=sprite/BG palette, 1=tile palette)
        //   bit 12:    priority (1=in front of sprites)
        //   bits 13-15: unused

        // Apply horizontal scroll
        // R0.D6: inhibit H-scroll for the top 2 character rows (lines 0-15)
        const bool h_inhibit = (regs_[reg::R0] & 0x40) && line_ < 16;
        const uint8_t scroll_x = h_inhibit ? 0 : this->scroll_x_;
        const int scrolled_col = (col - (scroll_x >> 3)) & 0x1F;

        // Apply vertical scroll per-column
        // R0.D7: inhibit V-scroll for the rightmost 8 columns (24-31)
        // Nametable height: 28 rows (192/224 lines) or 32 rows (240 lines)
        const bool v_inhibit = (regs_[reg::R0] & 0x80) && col >= 24;
        const uint8_t scroll_y = v_inhibit ? 0 : this->scroll_y_;
        const int nt_rows = (active_lines_ == 240) ? 32 : 28;
        int scrolled_row = (static_cast<int>(line_) + scroll_y) % (nt_rows * 8);
        const int tile_row_s = scrolled_row >> 3;
        const int fine_y_s = scrolled_row & 7;

        // Name table base (from R2, different layout in mode 4)
        const uint16_t nt_base = static_cast<uint16_t>((regs_[reg::R2] & 0x0E) << 10);
        const uint16_t nt_addr = nt_base + tile_row_s * 64 + scrolled_col * 2;

        const uint8_t lo = vram_[nt_addr & mask];
        const uint8_t hi = vram_[(nt_addr + 1) & mask];
        const uint16_t entry = static_cast<uint16_t>(lo | (hi << 8));

        const uint16_t pattern_idx = entry & 0x01FF;
        const bool h_flip = (entry >> 9) & 1;
        const bool v_flip = (entry >> 10) & 1;
        const uint8_t palette_bank = ((entry >> 11) & 1) ? 16 : 0;
        const bool priority = (entry >> 12) & 1;

        // Pattern data: 4 bytes per row, 32 bytes per tile
        int row_in_tile = v_flip ? (7 - fine_y_s) : fine_y_s;
        const uint16_t pat_addr = pattern_idx * 32 + row_in_tile * 4;

        // Read 4 bitplanes
        const uint8_t bp0 = vram_[pat_addr & mask];
        const uint8_t bp1 = vram_[(pat_addr + 1) & mask];
        const uint8_t bp2 = vram_[(pat_addr + 2) & mask];
        const uint8_t bp3 = vram_[(pat_addr + 3) & mask];

        // Store in pattern latches (we'll decode per-pixel in emit)
        // Pack bitplanes into 4 bytes for per-pixel extraction
        bg_.pattern_latch = bp0;  // Use pattern_latch for bp0
        bg_.color_latch = bp1;    // Repurpose color_latch for bp1
        bg_.mc_left = bp2;        // Repurpose mc_left for bp2
        bg_.mc_right = bp3;       // Repurpose mc_right for bp3
        bg_.fg_color = palette_bank;
        bg_.bg_color = h_flip ? 1 : 0;  // Flag h_flip in bg_color
        bg_.priority = priority;         // BG-over-sprite priority
    }
}

template <const VDPTraits& Traits>
void tms9918_t<Traits>::load_bg_shifter_sega() {
    // Sega mode 4: no traditional shift register — pixel extraction
    // is done per-pixel from the stored bitplane data.
    // Nothing to do here; data is already in the latches.
}

template <const VDPTraits& Traits>
uint8_t tms9918_t<Traits>::emit_bg_pixel_sega() {
    if constexpr (Traits.is_sega()) {
        // Extract one pixel from 4-bitplane data
        const uint8_t sub = bg_.pixel_in_char;
        const uint8_t actual_sub = (sub == 0) ? 7 : (sub - 1);
        const bool h_flip = bg_.bg_color != 0;
        const int bit = h_flip ? actual_sub : (7 - actual_sub);

        const uint8_t bp0 = bg_.pattern_latch;
        const uint8_t bp1 = bg_.color_latch;
        const uint8_t bp2 = bg_.mc_left;
        const uint8_t bp3 = bg_.mc_right;

        const uint8_t color_idx = static_cast<uint8_t>(
            ((bp0 >> bit) & 1) |
            (((bp1 >> bit) & 1) << 1) |
            (((bp2 >> bit) & 1) << 2) |
            (((bp3 >> bit) & 1) << 3));

        return color_idx + bg_.fg_color;  // fg_color holds palette bank offset
    }
    return backdrop_color();
}
