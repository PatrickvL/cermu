/*
 * ted7360.cpp — TED 7360/8360 cycle-accurate implementation
 *
 * Cycle-accurate emulation of the TED video/sound/IO chip using a state-machine
 * architecture driven by x_cycle position.  Each call to ted7360_tick_phi1()
 * processes one CPU cycle (= 2 TED single-clock cycles) and outputs 8 pixels.
 *
 * Key differences from VIC-II implementation:
 *   - No sprites: no per-cycle callback table needed, state enum suffices
 *   - TED fetches character AND color data itself (no separate color RAM chip)
 *   - 128 colors (16 hues × 8 luminances) vs VIC-II's 16 fixed colors
 *   - 114 TED clocks per line (57 CPU cycles) vs 63/65 VIC-II cycles
 *   - Banking latches at $FF3E/$FF3F for ROM/RAM switching
 *   - Reverse screen mode (RVS bit in $FF07) — per-character inversion
 *   - Hardware text blink (color attribute bit 7)
 *   - Timer 1 auto-reloads; Timers 2/3 wrap to $FFFF (no auto-reload)
 *
 * Screen memory layout ($800 bytes at screen_base):
 *   screen_base + $000..$3FF = colour/attribute memory
 *     Text: bits 3-0 = hue, bits 6-4 = luminance, bit 7 = blink
 *     Bitmap: bits 4-6 = "off" luminance, bits 0-2 = "on" luminance
 *   screen_base + $400..$7FF = screen code memory
 *     Text: character indices (8-bit, or 7-bit + inversion flag if RVS)
 *     Bitmap: bits 3-0 = "off" hue, bits 7-4 = "on" hue
 *
 * Memory access model (matching VIC-II phi1/phi2):
 *   PHI1: TED reads character generator / bitmap data (g-access)
 *         TED directly reads colour/attribute data on DMA lines
 *   PHI2: CPU has bus (unless stolen by TED DMA)
 *         On DMA lines, screen codes delivered from memory system (c-access)
 */

#include "ted7360.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// TED 7360 COLOR PALETTE — 128 colors (16 hues × 8 luminances)
// ============================================================================
// The TED generates 121 unique colors from 16 hues at 8 luminance levels.
// Hue 0 (black) is luminance-independent, giving 1 + 15*8 = 121 visual colors.
// Register color format: bits 6-4 = luminance (0-7), bits 3-0 = hue (0-15).
//
// The 128-entry palette is indexed directly by the 7-bit register value
// (luminance << 4 | hue).  Entries with hue=0 all map to black regardless
// of luminance.
//
// Palette values derived from Levente Hársfalvi's TED color measurements
// and cross-referenced with VICE's ted-color.c.

// Base hue RGB values at maximum luminance (luminance 7)
static const uint8_t ted_hue_r[16] = {
      0, 255, 109,  41, 145,  41,  41, 178,
    145, 109,  182, 109,  78, 109, 109, 150
};
static const uint8_t ted_hue_g[16] = {
      0, 255,  41, 178,  41, 145,  41, 178,
     72,  41,  109, 109,  78, 178,  41, 150
};
static const uint8_t ted_hue_b[16] = {
      0, 255,  41,  73, 145,  41, 178,  41,
     41, 109,   41, 178,  78,  73, 178, 150
};

// Luminance scaling factors (0-7 → approximate multiplier × 255)
// Index 0 is darkest (but not zero — that's black via hue 0),
// index 7 is brightest.
static const uint8_t ted_lum_scale[8] = {
     0,  40,  64,  96, 128, 168, 212, 255
};

// Pre-computed 128-entry palette (RGBA, alpha=0xFF)
static uint32_t ted_palette[TED_NUM_COLORS];
static bool ted_palette_initialized = false;

static void ted_init_palette(void) {
    if (ted_palette_initialized) return;
    for (int lum = 0; lum < 8; lum++) {
        for (int hue = 0; hue < 16; hue++) {
            int idx = (lum << 4) | hue;
            if (hue == 0) {
                // Hue 0 = black at all luminances
                ted_palette[idx] = 0xFF000000;
            } else {
                // Scale base hue by luminance
                uint8_t scale = ted_lum_scale[lum];
                uint8_t r = (uint8_t)((ted_hue_r[hue] * scale) / 255);
                uint8_t g = (uint8_t)((ted_hue_g[hue] * scale) / 255);
                uint8_t b = (uint8_t)((ted_hue_b[hue] * scale) / 255);
                ted_palette[idx] = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
            }
        }
    }
    ted_palette_initialized = true;
}

// Convert 7-bit TED color register value to palette index
static inline uint8_t ted_color_index(uint8_t color_reg) {
    return color_reg & 0x7F;  // 7 bits: lum[6:4] | hue[3:0]
}

// Convert TED color register value to RGBA
static inline uint32_t ted_color_to_rgba(uint8_t color_reg) {
    return ted_palette[ted_color_index(color_reg)];
}

const uint32_t* ted7360_get_palette(void) {
    ted_init_palette();
    return ted_palette;
}

// ============================================================================
// INLINE UTILITY FUNCTIONS
// ============================================================================

// Extract graphics mode from control registers (ECM|BMM|MCM, 3 bits)
static inline uint8_t ted_get_graphics_mode(const ted7360_t* ted) {
    uint8_t cr1 = ted->registers.data[TED_REG_CONTROL1];
    uint8_t cr2 = ted->registers.data[TED_REG_CONTROL2];
    return ((cr1 & TED_CR1_ECM) ? 4 : 0) |
           ((cr1 & TED_CR1_BMM) ? 2 : 0) |
           ((cr2 & TED_CR2_MCM) ? 1 : 0);
}

// Extract 9-bit raster compare value from register state
// Raster compare bit 8 is in $FF1A bit 0, bits 7-0 are in $FF1B
static inline uint16_t ted_get_raster_compare(const ted7360_t* ted) {
    return ((ted->registers.data[TED_REG_CHARPOS_HI] & 0x01) << 8) |
            ted->registers.data[TED_REG_RASTER_LO];
}

// ============================================================================
// MEMORY MAPPING — address calculation from registers
// ============================================================================

// Update memory unit addresses from current register values.
// Called when $FF12, $FF13 or $FF14 are written.
//
// $FF12 (MEM_CTRL): bits 2-3 select character ROM bank (0-3 = internal char ROM)
//                   bit 2: selects which half of ROM
// $FF13 (CHAR_HI):  bits 2-7 = character base address high bits (A10..A15)
//                   With bit 2 of $FF12 for A16 (ROM/RAM select)
// $FF14 (BITMAP_ADDR): bits 3-7 = screen/bitmap base address A10..A14
//                      bit 2 = screen A15
//                      bits 0-1 = unused
//
// For the TED, the screen base address comes from $FF14 bits 3-7 (× $400),
// character base from $FF13 bits 2-7 (× $400), and bitmap mode uses
// $FF14 bit 3 for the 8K bitmap base.
static void ted_update_memory_addresses(ted7360_t* ted) {
    uint8_t mem_ctrl = ted->registers.data[TED_REG_MEM_CTRL];
    uint8_t char_hi  = ted->registers.data[TED_REG_CHAR_HI];
    uint8_t bmp_addr = ted->registers.data[TED_REG_BITMAP_ADDR];

    // Screen base address:
    // $FF14 bits 3-7 select the screen matrix base in 1K steps (bits 10..14)
    // $FF14 bit 2 = bit 15
    ted->memory.screen_base = ((uint16_t)(bmp_addr & 0xF8) << 8);

    // Character base address:
    // $FF13 bits 2-7 select character generator address bits A10..A15
    // When bit 2 of $FF12 is clear AND bit 7 of $FF13 is clear, use internal ROM
    ted->memory.char_base = ((uint16_t)(char_hi & 0xFC) << 8);

    // Bitmap base address:
    // In bitmap mode, $FF12 bit 5 selects 8K base (0x0000 or 0x2000)
    // Combined with screen base for full addressing
    // $FF14 bit 3 provides the A13 bit for bitmap data
    ted->memory.bitmap_base = (bmp_addr & 0x08) ? 0x2000 : 0x0000;

    (void)mem_ctrl;  // Used for ROM bank selection, handled in system layer
}

// ============================================================================
// BORDER LOGIC — update comparison limits from registers
// ============================================================================

static inline void ted_update_border_limits(ted_border_unit_t* border,
                                             uint8_t cr1, uint8_t cr2) {
    // Vertical: RSEL selects 25-row or 24-row display window
    border->top    = (cr1 & TED_CR1_RSEL) ? TED_25ROW_START_LINE : TED_24ROW_START_LINE;
    border->bottom = (cr1 & TED_CR1_RSEL) ? TED_25ROW_STOP_LINE  : TED_24ROW_STOP_LINE;

    // Horizontal: CSEL selects 40-column or 38-column display window
    border->left  = (cr2 & TED_CR2_CSEL) ? TED_40COL_LEFT_BORDER_PX  : TED_38COL_LEFT_BORDER_PX;
    border->right = (cr2 & TED_CR2_CSEL) ? TED_40COL_RIGHT_BORDER_PX : TED_38COL_RIGHT_BORDER_PX;
}

// ============================================================================
// DMA LINE (BAD LINE) DETECTION
// ============================================================================
// A DMA line occurs when:
//   1. Raster counter is in the DMA range (0 to $CB)
//   2. Lower 3 bits of raster counter match YSCROLL
//   3. DEN bit is set (display enabled)
//
// This is checked at specific points during the line, similar to VIC-II bad lines.

static void ted_update_dma_condition(ted7360_t* ted) {
    uint16_t raster = ted->timing.raster_counter;
    uint8_t cr1 = ted->registers.data[TED_REG_CONTROL1];

    // Check DEN latch: DEN must have been set at some point to enable DMA
    if (cr1 & TED_CR1_DEN) {
        ted->video_logic.den_latched = true;
    }

    // DMA lines only in the display range
    if (raster <= TED_LAST_DMA_LINE && ted->video_logic.den_latched) {
        uint8_t yscroll = cr1 & TED_CR1_YSCROLL_MASK;
        ted->video_logic.is_dma_line = ((raster & 0x07) == yscroll);

        // Once a DMA line condition is detected, latch it for the entire line
        if (ted->video_logic.is_dma_line) {
            ted->video_logic.display_state = true;
            ted->video_logic.dma_line_occurred = true;
        }
    } else {
        ted->video_logic.is_dma_line = false;
    }
}

// ============================================================================
// RASTER IRQ — edge-triggered
// ============================================================================

static inline void ted_check_raster_interrupt(ted7360_t* ted) {
    if (ted->timing.raster_counter == ted->timing.raster_compare) {
        ted->irq_status |= TED_IRQ_RASTER;
    }
}

// ============================================================================
// TIMER TICK — counts down once per CPU cycle
// ============================================================================
// All three TED timers count down at the CPU clock rate (not TED clock rate).
// Timer 1 auto-reloads from its latch on underflow.
// Timers 2 and 3 do NOT auto-reload — they wrap from 0 to $FFFF and keep
// counting.  All three trigger their respective IRQ on underflow.
// (Source: cbmmuseum — "Der erste Timer lädt seinen Startwert wieder wenn er
// 0 erreicht, die anderen 2 laufen einfach so weiter.")

static inline void ted_tick_timers(ted7360_t* ted) {
    // Timer 1 — auto-reload from latch on underflow
    if (ted->timer1.counter == 0) {
        ted->irq_status |= TED_IRQ_TIMER1;
        ted->timer1.counter = ted->timer1.latch;
    } else {
        ted->timer1.counter--;
    }

    // Timer 2 — NO auto-reload, wraps to $FFFF
    if (ted->timer2.counter == 0) {
        ted->irq_status |= TED_IRQ_TIMER2;
        ted->timer2.counter = 0xFFFF;
    } else {
        ted->timer2.counter--;
    }

    // Timer 3 — NO auto-reload, wraps to $FFFF
    if (ted->timer3.counter == 0) {
        ted->irq_status |= TED_IRQ_TIMER3;
        ted->timer3.counter = 0xFFFF;
    } else {
        ted->timer3.counter--;
    }
}

// ============================================================================
// PIXEL SEQUENCER — 8 pixels per CPU cycle
// ============================================================================
// The TED pixel sequencer handles border flip-flops, XSCROLL delay, shift
// register management, and mode-dependent color selection.  It processes
// exactly 8 pixels per CPU cycle, matching the VIC-II model.
//
// Key differences from VIC-II:
//   - 128-color palette (7-bit color values from registers)
//   - Reverse mode (RVS bit inverts foreground/background in text modes)
//   - No sprite overlay — only graphics + border layers

static void ted_pixel_sequencer(ted7360_t* ted) {
    const uint16_t x_pixel = ted->timing.x_pixel;
    const uint16_t raster = ted->timing.raster_counter;
    const uint8_t cr1 = ted->registers.data[TED_REG_CONTROL1];
    const bool den_set = (cr1 & TED_CR1_DEN) != 0;

    // Border limits
    const uint16_t border_left  = ted->border.left;
    const uint16_t border_right = ted->border.right;
    const uint16_t border_top   = ted->border.top;
    const uint16_t border_bottom = ted->border.bottom;

    // Border color (7-bit register value)
    const uint8_t border_color = ted_color_index(ted->registers.data[TED_REG_BORDER]);
    const uint8_t bg0_color = ted_color_index(ted->registers.data[TED_REG_COLOR_BG0]);

    ted_sequencer_unit_t* seq = &ted->sequencer;

    // Track per-pixel border state (border transitions can happen mid-cycle)
    bool per_pixel_in_border[8];

    for (int pixel = 0; pixel < 8; pixel++) {
        uint16_t px = x_pixel + (uint16_t)pixel;

        // --- Border flip-flop logic (same rules as VIC-II) ---
        // Rule 1: Right comparison → set main border
        if (px == border_right) {
            ted->border.main_ff = true;
        }

        // Rules at left edge
        if (px == border_left) {
            // Rule 4: Left + bottom → set vertical border
            if (raster == border_bottom) {
                ted->border.vert_ff = true;
            }
            // Rule 5: Left + top + DEN → clear vertical border
            else if (raster == border_top && den_set) {
                ted->border.vert_ff = false;
            }
            // Rule 6: Left + vert_ff clear → clear main border
            if (!ted->border.vert_ff) {
                // Border is opening — initialize XSCROLL and column counter
                if (ted->border.main_ff) {
                    seq->xscroll = ted->registers.data[TED_REG_CONTROL2] & TED_CR2_XSCROLL_MASK;
                    seq->pixel_in_char = 0;
                    seq->display_vmli = 0;
                }
                ted->border.main_ff = false;
            }
        }

        per_pixel_in_border[pixel] = ted->border.main_ff || ted->border.vert_ff;

        // Output border color for border pixels
        if (per_pixel_in_border[pixel]) {
            // Write to color line buffer
            if (ted->pixel.color_line && px < TED_VISIBLE_WIDTH) {
                ted->pixel.color_line[px] = border_color;
            }
        }
    }

    // --- Display area pixel sequencing (non-border pixels) ---
    if (!ted->video_logic.display_state) {
        // Idle state — output background color for non-border pixels
        for (int pixel = 0; pixel < 8; pixel++) {
            if (per_pixel_in_border[pixel]) continue;
            uint16_t px = x_pixel + (uint16_t)pixel;
            if (ted->pixel.color_line && px < TED_VISIBLE_WIDTH) {
                ted->pixel.color_line[px] = bg0_color;
            }
        }
        return;
    }

    // Update graphics mode from registers
    seq->graphics_mode = ted_get_graphics_mode(ted);

    for (int pixel = 0; pixel < 8; pixel++) {
        if (per_pixel_in_border[pixel]) continue;

        uint16_t px = x_pixel + (uint16_t)pixel;
        if (px >= TED_VISIBLE_WIDTH) continue;

        uint8_t color_idx;

        // XSCROLL delay — output bg0 color during scroll offset
        if (seq->xscroll > 0) {
            seq->xscroll--;
            if (ted->pixel.color_line) {
                ted->pixel.color_line[px] = bg0_color;
            }
            continue;
        }

        // Reload shift register when starting a new character
        if (seq->pixel_in_char == 0 && seq->display_vmli < TED_SCREEN_TEXTCOLS) {
            seq->shift_reg = seq->char_data[seq->display_vmli];
            seq->active_display_column = seq->display_vmli;
            seq->display_vmli++;
        }
        const uint8_t vmli = seq->active_display_column;

        // Extract pixel from shift register based on graphics mode
        uint8_t pixel_bits = 0;

        // --- Fetch attribute and screen code for this character position ---
        // color_line[] = attribute/color memory (first $400): [blink|lum2|lum1|lum0|hue3|hue2|hue1|hue0]
        // screen_line[] = screen code memory (second $400): character indices
        const uint8_t attr = ted->video_data.color_line[vmli];
        const uint8_t screen_code = ted->video_data.screen_line[vmli];

        switch (seq->graphics_mode) {
            case TED_GM_STANDARD_TEXT: {
                // Standard text mode:
                // Foreground = color from attribute (bits 6-0), background = BG0
                // When RVS mode is enabled ($FF07 bit 7), bit 7 of the screen
                // code selects per-character inversion (fg/bg swap).
                // When RVS is off, all 8 bits are the character index.
                // Blink: if attribute bit 7 is set, character blinks (alternates
                // between visible and BG0 based on flash counter).
                pixel_bits = (seq->shift_reg >> 7) & 1;

                // Per-character reverse: RVS mode + bit 7 of screen code
                if (ted->reverse_mode && (screen_code & 0x80)) {
                    pixel_bits ^= 1;
                }

                // Blink: attribute bit 7 → when flash phase is off, show BG0
                bool blink_hide = (attr & 0x80) && !(ted->flash_counter & 0x10);

                if (blink_hide) {
                    color_idx = bg0_color;
                } else if (pixel_bits) {
                    // Foreground — 7-bit color from attribute (bits 6-0)
                    color_idx = ted_color_index(attr);
                } else {
                    color_idx = bg0_color;
                }
                seq->shift_reg <<= 1;
                break;
            }

            case TED_GM_MULTICOLOR_TEXT: {
                // Multicolor text mode:
                // If bit 3 of color attribute is set, use multicolor (2 bits/pixel)
                // Otherwise, standard text (1 bit/pixel).
                // Inversion and blinking are disabled in MCM.
                if (attr & 0x08) {
                    // Multicolor: 2 bits per pixel, displayed double-width
                    pixel_bits = (seq->shift_reg >> 6) & 3;
                    switch (pixel_bits) {
                        case 0: color_idx = bg0_color; break;
                        case 1: color_idx = ted_color_index(ted->registers.data[TED_REG_COLOR_BG1]); break;
                        case 2: color_idx = ted_color_index(ted->registers.data[TED_REG_COLOR_BG2]); break;
                        case 3: color_idx = ted_color_index(attr & 0x77); break; // attribute color, mask bit 3
                    }
                    if (seq->pixel_in_char & 1) {
                        seq->shift_reg <<= 2;
                    }
                } else {
                    // Standard character in multicolor mode
                    pixel_bits = (seq->shift_reg >> 7) & 1;
                    if (pixel_bits) {
                        color_idx = ted_color_index(attr);
                    } else {
                        color_idx = bg0_color;
                    }
                    seq->shift_reg <<= 1;
                }
                break;
            }

            case TED_GM_STANDARD_BITMAP: {
                // Standard (hires) bitmap mode:
                // The $800-byte color area is split into:
                //   color_line[] = intensity memory (first $400):
                //     bits 4-6 = luminance for "off" pixel
                //     bits 0-2 = luminance for "on" pixel
                //     bits 3, 7 = unused
                //   screen_line[] = colour code memory (second $400):
                //     bits 0-3 = hue for "off" pixel
                //     bits 4-7 = hue for "on" pixel
                // The 7-bit TED color is constructed: (luminance << 4) | hue
                pixel_bits = (seq->shift_reg >> 7) & 1;
                if (pixel_bits) {
                    // "on" pixel: hue from upper nibble of colour, lum from bits 0-2 of intensity
                    uint8_t hue = (screen_code >> 4) & 0x0F;
                    uint8_t lum = attr & 0x07;
                    color_idx = (lum << 4) | hue;
                } else {
                    // "off" pixel: hue from lower nibble of colour, lum from bits 4-6 of intensity
                    uint8_t hue = screen_code & 0x0F;
                    uint8_t lum = (attr >> 4) & 0x07;
                    color_idx = (lum << 4) | hue;
                }
                seq->shift_reg <<= 1;
                break;
            }

            case TED_GM_MULTICOLOR_BITMAP: {
                // Multicolor bitmap: 2 bits per pixel, double-width
                // Same intensity/colour split as hires bitmap:
                //   00 = background color (BG0 register)
                //   01 = "off" color (same construction as hires off)
                //   10 = "on" color (same construction as hires on)
                //   11 = another background color (BG1 register)
                pixel_bits = (seq->shift_reg >> 6) & 3;
                switch (pixel_bits) {
                    case 0: color_idx = bg0_color; break;
                    case 1: {
                        // "off": hue from lower nibble colour, lum from bits 4-6 intensity
                        uint8_t hue = screen_code & 0x0F;
                        uint8_t lum = (attr >> 4) & 0x07;
                        color_idx = (lum << 4) | hue;
                        break;
                    }
                    case 2: {
                        // "on": hue from upper nibble colour, lum from bits 0-2 intensity
                        uint8_t hue = (screen_code >> 4) & 0x0F;
                        uint8_t lum = attr & 0x07;
                        color_idx = (lum << 4) | hue;
                        break;
                    }
                    case 3: color_idx = ted_color_index(ted->registers.data[TED_REG_COLOR_BG1]); break;
                }
                if (seq->pixel_in_char & 1) {
                    seq->shift_reg <<= 2;
                }
                break;
            }

            case TED_GM_ECM_TEXT: {
                // Extended Color Mode text:
                // Upper 2 bits of screen code select BG0-BG3, lower 6 bits = char index
                // Character generator address has A9-A10 held low.
                // Inversion and blinking are disabled in ECM.
                pixel_bits = (seq->shift_reg >> 7) & 1;
                if (pixel_bits) {
                    color_idx = ted_color_index(attr);
                } else {
                    // Background selected by upper 2 bits of screen code
                    uint8_t bg_sel = (screen_code >> 6) & 0x03;
                    color_idx = ted_color_index(ted->registers.data[TED_REG_COLOR_BG0 + bg_sel]);
                }
                seq->shift_reg <<= 1;
                break;
            }

            default:
                // Invalid modes (ECM+BMM, ECM+MCM, ECM+BMM+MCM): output black
                color_idx = 0;
                seq->shift_reg <<= 1;
                break;
        }

        seq->pixel_in_char = (seq->pixel_in_char + 1) & 7;

        if (ted->pixel.color_line) {
            ted->pixel.color_line[px] = color_idx;
        }
    }
}

// ============================================================================
// SCANLINE FLUSH — copy color line buffer to framebuffer
// ============================================================================

static void ted_flush_line(ted7360_t* ted, uint16_t raster_line) {
    if (!ted->pixel.framebuffer || !ted->pixel.color_line) return;
    if (raster_line >= (uint16_t)ted->pixel.fb_height) return;

    const uint32_t* palette = ted7360_get_palette();
    uint32_t* fb_row = ted->pixel.framebuffer + raster_line * ted->pixel.fb_width;
    int width = (ted->pixel.fb_width < TED_VISIBLE_WIDTH) ? ted->pixel.fb_width : TED_VISIBLE_WIDTH;

    for (int x = 0; x < width; x++) {
        fb_row[x] = palette[ted->pixel.color_line[x]];
    }
}

// ============================================================================
// TIMING ADVANCE — move to next CPU cycle, end-of-line / end-of-frame handling
// ============================================================================

static void ted_timing_advance(ted7360_t* ted) {
    // Common case: advance within current line
    if (ted->timing.x_cycle < ted->timing.cpu_cycles_per_line - 1) {
        ted->timing.x_cycle++;
        ted->timing.x_pixel = ted->timing.x_cycle * 8;
        return;
    }

    // --- End of line ---

    // Flush completed scanline to framebuffer
    ted_flush_line(ted, ted->timing.raster_counter);

    // Reset horizontal counter
    ted->timing.x_cycle = 0;
    ted->timing.x_pixel = 0;

    // Clear DMA line latch for the new line
    ted->video_logic.dma_line_occurred = false;

    // Advance raster counter
    uint16_t new_raster = ted->timing.raster_counter + 1;
    if (new_raster >= ted->timing.lines_per_frame) {
        new_raster = 0;

        // --- End of frame ---
        ted->timing.frame_count++;
        ted->flash_counter = (ted->flash_counter + 1) & 0x3F;
        ted->cursor_visible = (ted->flash_counter & 0x10) != 0;

        // Reset video counters for new frame
        ted->video_logic.vcbase = 0;
        ted->video_logic.vc = 0;
        ted->video_logic.rc = 0;
        ted->video_logic.display_state = false;
        ted->video_logic.den_latched = false;

        // Reset border flip-flops
        ted->border.vert_ff = true;
        ted->border.main_ff = true;
    }

    ted->timing.raster_counter = new_raster;

    // Check raster interrupt on line transition (edge-triggered)
    ted_check_raster_interrupt(ted);

    // Clear color line buffer for new scanline
    if (ted->pixel.color_line) {
        memset(ted->pixel.color_line, 0, TED_VISIBLE_WIDTH);
    }
}

// ============================================================================
// CREATE / DESTROY / RESET
// ============================================================================

ted7360_t* ted7360_create(const ted7360_desc_t* desc) {
    ted_init_palette();

    ted7360_t* ted = new ted7360_t();

    ted->timing.is_pal = desc ? desc->is_pal : true;
    ted->keyboard_scan = desc ? desc->keyboard_scan : nullptr;
    ted->keyboard_user_data = desc ? desc->keyboard_user_data : nullptr;

    if (desc && desc->mem_read) {
        ted->bus.mem_read = desc->mem_read;
        ted->bus.mem_read_user_data = desc->mem_read_user_data;
    }

    if (ted->timing.is_pal) {
        ted->timing.lines_per_frame = TED_PAL_LINES_PER_FRAME;
        ted->timing.cpu_cycles_per_line = TED_PAL_CPU_CYCLES_PER_LINE;
    } else {
        ted->timing.lines_per_frame = TED_NTSC_LINES_PER_FRAME;
        ted->timing.cpu_cycles_per_line = TED_NTSC_CPU_CYCLES_PER_LINE;
    }

    // Allocate line buffer for pixel color indices
    ted->pixel.color_line = new uint8_t[TED_VISIBLE_WIDTH]();

    ted7360_reset(ted);
    return ted;
}

void ted7360_destroy(ted7360_t* ted) {
    if (!ted) return;
    delete[] ted->pixel.color_line;
    ted->pixel.color_line = nullptr;
    delete ted;
}

void ted7360_reset(ted7360_t* ted) {
    if (!ted) return;

    // Preserve configuration that was set at create time
    bool is_pal = ted->timing.is_pal;
    uint16_t lines = ted->timing.lines_per_frame;
    uint8_t cycles = ted->timing.cpu_cycles_per_line;
    ted_keyboard_scan_fn kb = ted->keyboard_scan;
    void* kb_data = ted->keyboard_user_data;
    ted_mem_read_fn mem = ted->bus.mem_read;
    void* mem_data = ted->bus.mem_read_user_data;
    uint8_t* color_line = ted->pixel.color_line;
    uint32_t* fb = ted->pixel.framebuffer;
    int fb_w = ted->pixel.fb_width;
    int fb_h = ted->pixel.fb_height;

    // Zero everything
    memset(&ted->registers, 0, sizeof(ted->registers));
    memset(&ted->timing, 0, sizeof(ted->timing));
    memset(&ted->video_logic, 0, sizeof(ted->video_logic));
    memset(&ted->video_data, 0, sizeof(ted->video_data));
    memset(&ted->sequencer, 0, sizeof(ted->sequencer));
    memset(&ted->border, 0, sizeof(ted->border));
    memset(&ted->memory, 0, sizeof(ted->memory));
    memset(&ted->sound, 0, sizeof(ted->sound));
    memset(&ted->bus, 0, sizeof(ted->bus));

    // Restore configuration
    ted->timing.is_pal = is_pal;
    ted->timing.lines_per_frame = lines;
    ted->timing.cpu_cycles_per_line = cycles;
    ted->keyboard_scan = kb;
    ted->keyboard_user_data = kb_data;
    ted->bus.mem_read = mem;
    ted->bus.mem_read_user_data = mem_data;
    ted->pixel.color_line = color_line;
    ted->pixel.framebuffer = fb;
    ted->pixel.fb_width = fb_w;
    ted->pixel.fb_height = fb_h;

    // Default register values after reset
    ted->registers.data[TED_REG_CONTROL1] = 0x00;  // Display disabled
    ted->registers.data[TED_REG_CONTROL2] = ted->timing.is_pal ? 0x00 : TED_CR2_PAL_NTSC;
    ted->registers.data[TED_REG_IRQ_STATUS] = 0x00;
    ted->registers.data[TED_REG_IRQ_MASK] = 0x00;
    ted->registers.data[TED_REG_BORDER] = 0x00;     // Black border
    ted->registers.data[TED_REG_COLOR_BG0] = 0x00;  // Black background

    // Timers: reset to max
    ted->timer1.counter = 0xFFFF;
    ted->timer1.latch = 0xFFFF;
    ted->timer2.counter = 0xFFFF;
    ted->timer2.latch = 0xFFFF;
    ted->timer3.counter = 0xFFFF;
    ted->timer3.latch = 0xFFFF;

    // Raster
    ted->timing.raster_counter = 0;
    ted->timing.raster_compare = 0;
    ted->timing.x_cycle = 0;
    ted->timing.x_pixel = 0;
    ted->timing.frame_count = 0;

    // IRQ
    ted->irq_status = 0;
    ted->irq_mask = 0;

    // Memory banking: ROM enabled after reset
    ted->rom_enabled = true;

    // Border: start fully in border
    ted->border.vert_ff = true;
    ted->border.main_ff = true;
    ted_update_border_limits(&ted->border,
                             ted->registers.data[TED_REG_CONTROL1],
                             ted->registers.data[TED_REG_CONTROL2]);

    // Keyboard
    ted->keyboard_latch = 0xFF;

    // Flash
    ted->flash_counter = 0;
    ted->cursor_visible = false;
    ted->reverse_mode = false;

    // Memory addresses
    ted_update_memory_addresses(ted);

    // Clear line buffer
    if (ted->pixel.color_line) {
        memset(ted->pixel.color_line, 0, TED_VISIBLE_WIDTH);
    }
}

// ============================================================================
// PHI1 TICK — main TED processing (one CPU cycle)
// ============================================================================
//
// Execution order per CPU cycle (matching VIC-II model):
//   1. Determine line state from x_cycle position
//   2. Perform PHI1 memory access (g-access for chargen/bitmap data)
//   3. Store graphics data in line buffer
//   4. Increment VC/VMLI after g-access
//   5. Pixel sequencer (8 pixels)
//   6. Timer countdown
//   7. IRQ and BA/AEC signal update
//   8. Timing advance (x_cycle++)
//   9. DMA condition check
//  10. Set up PHI2 bus address for DMA c-access (if applicable)

bus_state_t ted7360_tick_phi1(ted7360_t* ted, bus_state_t bus_state) {
    if (!ted) return bus_state;

    const uint8_t x = ted->timing.x_cycle;
    const uint16_t raster = ted->timing.raster_counter;

    // ===== STEP 1: Determine if this cycle is a DMA cycle =====
    // DMA (character + color fetch) occurs during CPU cycles 4..46 on DMA lines.
    // The first 3 cycles (4,5,6) are the setup period where BA goes LOW but
    // AEC still follows φ2, similar to VIC-II's 3-cycle BA warning.
    // Actual character data transfers happen at cycles 7..46.
    const bool in_dma_window = (x >= TED_FETCH_CYCLE && x <= TED_FETCH_END_CYCLE);
    const bool dma_active = in_dma_window && ted->video_logic.dma_line_occurred;

    // BA signal: goes LOW during DMA window on DMA lines (3 cycles ahead)
    // Check if any cycle in [x, x+3] falls within the DMA window on a DMA line
    const bool ba_should_be_low = ted->video_logic.dma_line_occurred &&
                                  (x + 3 >= TED_FETCH_CYCLE) &&
                                  (x <= TED_FETCH_END_CYCLE);
    ted->bus.ba_low = ba_should_be_low;

    if (ba_should_be_low) {
        if (ted->bus.ba_low_count < 4) ted->bus.ba_low_count++;
        // Pull BA LOW on the bus
        bus_state &= ~BUS_BIT(BUS_BA_BIT);
        // After 3 cycles of BA low, AEC stays low during PHI2
        if (ted->bus.ba_low_count >= 3) {
            bus_state &= ~BUS_BIT(BUS_AEC_BIT);
        }
        // Copy BA to RDY (CPU halts on reads when RDY is LOW)
        bus_state &= ~BUS_BIT(BUS_RDY_BIT);
    } else {
        ted->bus.ba_low_count = 0;
        bus_state |= BUS_BIT(BUS_BA_BIT);
        bus_state |= BUS_BIT(BUS_RDY_BIT);
    }

    // ===== STEP 2: PHI1 memory access (g-access) =====
    // During display state, the TED reads character generator or bitmap data
    // at the address computed from current VC and RC.
    // Outside display state, an idle access to $FFFF occurs.
    const bool in_display_window = (x >= TED_FETCH_CYCLE + TED_DMA_SETUP_CYCLES &&
                                     x <= TED_FETCH_END_CYCLE);

    if (ted->video_logic.display_state && in_display_window) {
        uint16_t address;
        const uint8_t vmli = ted->video_logic.vmli;
        const uint8_t rc = ted->video_logic.rc;

        if (ted->sequencer.graphics_mode & 2) {
            // Bitmap mode: address = bitmap_base | (VC << 3) | RC
            uint16_t vc = (ted->video_logic.vcbase + vmli) & 0x3FF;
            address = ted->memory.bitmap_base | (vc << 3) | rc;
        } else {
            // Text mode: address = char_base | (screen_code << 3) | RC
            uint8_t screen_code = ted->video_data.screen_line[vmli < TED_SCREEN_TEXTCOLS ? vmli : 0];

            // RVS mode: bit 7 of screen code is the per-character inversion flag,
            // so only bits 0-6 are the actual character index (128 chars max)
            if (ted->reverse_mode) {
                screen_code &= 0x7F;
            }

            // ECM: hold address lines A9-A10 low
            if (ted->sequencer.graphics_mode & 4) {
                screen_code &= 0x3F;
            }

            address = ted->memory.char_base | ((uint16_t)screen_code << 3) | rc;
        }

        // Perform memory read via callback
        uint8_t gdata = 0xFF;
        if (ted->bus.mem_read) {
            gdata = ted->bus.mem_read(ted->bus.mem_read_user_data, address);
        }

        // Store g-access data in character line buffer
        if (vmli < TED_SCREEN_TEXTCOLS) {
            ted->sequencer.char_data[vmli] = gdata;
        }

        // Increment VC and VMLI after g-access
        ted->video_logic.vmli++;
        ted->video_logic.vc = (ted->video_logic.vc + 1) & 0x3FF;
    }

    // ===== STEP 3: DMA c-access preparation (screen + color fetch) =====
    // On DMA lines, the TED fetches screen codes and color/attribute data
    // from the $800-byte screen block.  The block is organized as:
    //   screen_base + $000..$3FF = color/attribute memory (hue+lum+blink)
    //   screen_base + $400..$7FF = screen codes (character indices)
    // The screen codes are fetched via the memory system during PHI2
    // (c-access), while color/attribute data is read directly by TED.
    uint16_t c_access_address = 0;
    bool c_access_pending = false;

    if (dma_active && x >= TED_FETCH_CYCLE + TED_DMA_SETUP_CYCLES) {
        uint16_t vc = ted->video_logic.vc & 0x3FF;

        // Screen code read via PHI2 bus: screen_base + $400 + VC
        c_access_address = (ted->memory.screen_base + 0x0400) | vc;
        c_access_pending = true;

        // Color/attribute read directly by TED: screen_base + VC
        // Format: bits 3-0 = hue, bits 6-4 = luminance, bit 7 = blink
        if (ted->bus.mem_read && ted->video_logic.vmli <= TED_SCREEN_TEXTCOLS) {
            uint8_t vmli_for_color = ted->video_logic.vmli - 1;
            if (vmli_for_color < TED_SCREEN_TEXTCOLS) {
                uint16_t color_addr = ted->memory.screen_base | ((ted->video_logic.vcbase + vmli_for_color) & 0x3FF);
                ted->video_data.color_line[vmli_for_color] =
                    ted->bus.mem_read(ted->bus.mem_read_user_data, color_addr);
            }
        }
    }

    // ===== STEP 4: Pixel sequencer (8 pixels) =====
    if (ted->pixel.framebuffer && raster < (uint16_t)ted->pixel.fb_height) {
        ted_pixel_sequencer(ted);
    }

    // ===== STEP 5: Timer countdown =====
    ted_tick_timers(ted);

    // ===== STEP 6: IRQ signaling =====
    // TED IRQ is active-LOW. Assert by clearing IRQ bit.
    if (ted->irq_status & ted->irq_mask) {
        bus_state &= ~BUS_BIT(BUS_IRQ_BIT);
    }
    // Pull-up resistor model handles de-assertion (system default state)

    // ===== STEP 7: Timing advance =====
    // Store pending access type for PHI2 delivery
    ted->bus.pending_access = c_access_pending ? TED_ACCESS_C : TED_ACCESS_IDLE;
    ted->bus.pending_address = c_access_address;

    ted_timing_advance(ted);
    ted_update_dma_condition(ted);

    // ===== STEP 8: Handle RC and VCBASE updates at cycle 58 =====
    // At the end of the character display window:
    // - If RC == 7: VCBASE = VC, display_state = false (idle)
    // - Otherwise: RC++ if display_state is true
    if (x == 55) {  // Approximately equivalent to VIC-II cycle 58
        if (ted->video_logic.display_state) {
            if (ted->video_logic.rc == 7) {
                ted->video_logic.vcbase = ted->video_logic.vc;
                ted->video_logic.display_state = false;
            } else {
                ted->video_logic.rc = (ted->video_logic.rc + 1) & 7;
            }
        }

        // On DMA lines, reset RC to 0 (new character row starts)
        if (ted->video_logic.is_dma_line) {
            ted->video_logic.rc = 0;
            ted->video_logic.vc = ted->video_logic.vcbase;
            ted->video_logic.vmli = 0;
        }
    }

    // ===== STEP 9: Set up PHI2 bus address for DMA c-access =====
    if (c_access_pending) {
        BUS_SET_ADDR(bus_state, c_access_address);
        // Set RW high (read mode)
        bus_state |= BUS_BIT(BUS_RW_BIT);
    }

    return bus_state;
}

// ============================================================================
// PHI2 DELIVERY — process data from memory tick
// ============================================================================

void ted7360_tick_phi2(ted7360_t* ted, bus_state_t bus_state) {
    if (!ted) return;

    if (ted->bus.pending_access == TED_ACCESS_C) {
        // Screen matrix data delivered from memory system
        uint8_t data = BUS_GET_DATA(bus_state);
        uint8_t vmli = ted->video_logic.vmli;

        // vmli was incremented after g-access, so the c-access data
        // corresponds to vmli-1
        if (vmli > 0 && (vmli - 1) < TED_SCREEN_TEXTCOLS) {
            ted->video_data.screen_line[vmli - 1] = data;
        }
    }

    ted->bus.pending_access = TED_ACCESS_IDLE;

    // Update DMA condition at end of PHI2 (catches register writes by CPU)
    ted_update_dma_condition(ted);
}

// ============================================================================
// LEGACY SINGLE-TICK — backward compatibility wrapper
// ============================================================================
// The legacy tick() function was called at 2× CPU clock rate (once per TED
// single-clock cycle).  We now operate per CPU cycle, so legacy tick is called
// twice to maintain the same number of invocations, but only the first call
// does actual work.

static int ted_legacy_subcycle = 0;

void ted7360_tick(ted7360_t* ted) {
    if (!ted) return;

    // Alternate between PHI1 (actual work) and PHI2 (no-op for legacy callers)
    if (ted_legacy_subcycle == 0) {
        bus_state_t bus = 0;
        bus |= BUS_BIT(BUS_IRQ_BIT);  // IRQ de-asserted (active low, so set high)
        bus |= BUS_BIT(BUS_RW_BIT);   // Read mode
        bus |= BUS_BIT(BUS_BA_BIT);   // Bus available
        bus |= BUS_BIT(BUS_RDY_BIT);  // Ready

        ted7360_tick_phi1(ted, bus);
    } else {
        bus_state_t bus = 0;
        bus |= BUS_BIT(BUS_IRQ_BIT);
        bus |= BUS_BIT(BUS_RW_BIT);
        bus |= BUS_BIT(BUS_BA_BIT);
        bus |= BUS_BIT(BUS_RDY_BIT);
        BUS_SET_DATA(bus, 0xFF);

        ted7360_tick_phi2(ted, bus);
    }
    ted_legacy_subcycle ^= 1;
}

// ============================================================================
// REGISTER READ
// ============================================================================

uint8_t ted7360_read_register(ted7360_t* ted, uint8_t reg) {
    if (!ted) return 0xFF;

    reg &= 0x3F;  // Handle mirroring in $FF00-$FF3F range

    // Banking latches: $FF3E/$FF3F read as open bus
    if (reg >= 0x20) {
        if (reg == 0x3E || reg == 0x3F) return 0xFF;
        // Mirrored registers ($20-$3D map to $00-$1D)
        reg &= 0x1F;
    }

    switch (reg) {
        // Timer reads return current counter value (not latch)
        case TED_REG_TIMER1_LO: return ted->timer1.counter & 0xFF;
        case TED_REG_TIMER1_HI: return (ted->timer1.counter >> 8) & 0xFF;
        case TED_REG_TIMER2_LO: return ted->timer2.counter & 0xFF;
        case TED_REG_TIMER2_HI: return (ted->timer2.counter >> 8) & 0xFF;
        case TED_REG_TIMER3_LO: return ted->timer3.counter & 0xFF;
        case TED_REG_TIMER3_HI: return (ted->timer3.counter >> 8) & 0xFF;

        case TED_REG_KEYBOARD:
            // Scan keyboard matrix using column pattern in latch
            if (ted->keyboard_scan) {
                return ted->keyboard_scan(ted->keyboard_user_data, ted->keyboard_latch);
            }
            return 0xFF;  // No keys pressed

        case TED_REG_IRQ_STATUS:
            // Bit 7 = any enabled IRQ source is active
            return ted->irq_status | ((ted->irq_status & ted->irq_mask) ? TED_IRQ_ANY : 0);

        case TED_REG_IRQ_MASK:
            return ted->irq_mask;

        case TED_REG_RASTER_LO:
            return ted->timing.raster_counter & 0xFF;

        case TED_REG_CHARPOS_HI:
            // Bit 0 = raster counter bit 8, rest from register
            return (ted->registers.data[reg] & 0xFE) |
                   ((ted->timing.raster_counter >> 8) & 0x01);

        case TED_REG_HPOS:
            // Horizontal position: return current CPU cycle × 2 (TED clocks)
            return (ted->timing.x_cycle * 2) & 0xFF;

        case TED_REG_VPOS:
            return ted->timing.raster_counter & 0xFF;

        case TED_REG_FLASH:
            // Bits 7-0: flash counter (6 bits) in upper bits, raster compare in lower
            return (ted->flash_counter << 1) | (ted->registers.data[reg] & 0x01);

        default:
            return ted->registers.data[reg];
    }
}

// ============================================================================
// REGISTER WRITE
// ============================================================================

void ted7360_write_register(ted7360_t* ted, uint8_t reg, uint8_t data) {
    if (!ted) return;

    reg &= 0x3F;  // Handle mirroring

    // ROM/RAM banking latches (not mirrored)
    if (reg == 0x3E) {
        ted->rom_enabled = true;
        return;
    }
    if (reg == 0x3F) {
        ted->rom_enabled = false;
        return;
    }

    // Mirror writes above $1F to actual register range
    if (reg >= 0x20) {
        reg &= 0x1F;
    }

    switch (reg) {
        // Timer writes go to latch; high byte write also loads counter
        case TED_REG_TIMER1_LO:
            ted->timer1.latch = (ted->timer1.latch & 0xFF00) | data;
            ted->registers.data[reg] = data;
            break;
        case TED_REG_TIMER1_HI:
            ted->timer1.latch = (ted->timer1.latch & 0x00FF) | ((uint16_t)data << 8);
            ted->timer1.counter = ted->timer1.latch;
            ted->registers.data[reg] = data;
            break;

        case TED_REG_TIMER2_LO:
            ted->timer2.latch = (ted->timer2.latch & 0xFF00) | data;
            ted->registers.data[reg] = data;
            break;
        case TED_REG_TIMER2_HI:
            ted->timer2.latch = (ted->timer2.latch & 0x00FF) | ((uint16_t)data << 8);
            ted->timer2.counter = ted->timer2.latch;
            ted->registers.data[reg] = data;
            break;

        case TED_REG_TIMER3_LO:
            ted->timer3.latch = (ted->timer3.latch & 0xFF00) | data;
            ted->registers.data[reg] = data;
            break;
        case TED_REG_TIMER3_HI:
            ted->timer3.latch = (ted->timer3.latch & 0x00FF) | ((uint16_t)data << 8);
            ted->timer3.counter = ted->timer3.latch;
            ted->registers.data[reg] = data;
            break;

        case TED_REG_CONTROL1:
            ted->registers.data[reg] = data;
            ted_update_border_limits(&ted->border, data, ted->registers.data[TED_REG_CONTROL2]);
            // Update raster compare (bit 8 is in CHARPOS_HI bit 0)
            ted->timing.raster_compare = ted_get_raster_compare(ted);
            // DMA condition may change due to YSCROLL or DEN change
            ted_update_dma_condition(ted);
            break;

        case TED_REG_CONTROL2:
            ted->registers.data[reg] = data;
            ted_update_border_limits(&ted->border, ted->registers.data[TED_REG_CONTROL1], data);
            // Update reverse mode
            ted->reverse_mode = (data & TED_CR2_RVS) != 0;
            break;

        case TED_REG_KEYBOARD:
            ted->keyboard_latch = data;
            ted->registers.data[reg] = data;
            break;

        case TED_REG_IRQ_STATUS:
            // Writing 1 to bits clears the corresponding IRQ source
            ted->irq_status &= ~(data & TED_IRQ_CLEARABLE);
            break;

        case TED_REG_IRQ_MASK:
            ted->irq_mask = data & TED_IRQ_CLEARABLE;
            break;

        case TED_REG_CHARPOS_HI:
            ted->registers.data[reg] = data;
            // Bit 0 = raster compare bit 8
            ted->timing.raster_compare = ted_get_raster_compare(ted);
            break;

        case TED_REG_RASTER_LO:
            ted->registers.data[reg] = data;
            // Bits 7-0 of raster compare
            ted->timing.raster_compare = ted_get_raster_compare(ted);
            break;

        case TED_REG_MEM_CTRL:
        case TED_REG_CHAR_HI:
        case TED_REG_BITMAP_ADDR:
            ted->registers.data[reg] = data;
            ted_update_memory_addresses(ted);
            break;

        case TED_REG_ROM_RAM:
            ted->registers.data[reg] = data;
            // Bit 1: single-clock mode (FREEZE-like)
            // Other bits relate to banking and clock selection
            break;

        default:
            ted->registers.data[reg] = data;
            break;
    }
}

// ============================================================================
// IRQ QUERY
// ============================================================================

bool ted7360_irq_pending(const ted7360_t* ted) {
    if (!ted) return false;
    return (ted->irq_status & ted->irq_mask) != 0;
}

// ============================================================================
// FRAMEBUFFER
// ============================================================================

void ted7360_set_framebuffer(ted7360_t* ted, uint32_t* buffer, int width, int height) {
    if (!ted) return;
    ted->pixel.framebuffer = buffer;
    ted->pixel.fb_width = width;
    ted->pixel.fb_height = height;
}
