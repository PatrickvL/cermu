#include "chip/video/vic_ii/vicii_common.hpp"
#include "chip/memory/mos2114.hpp"
#include "core/system_lines.hpp"
#include "core/cermu.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>

using namespace vicii;

// ========================================================================================
// CONSTANTS AND STATIC DATA
// ========================================================================================

// VIC-II pipeline delay: Data fetched at position X is displayed 12 pixels later
// Documentation (vic-ii.txt line 905-906): "the read graphics data is not
// immediately displayed on the screen (there is a delay of 12 pixels)"
constexpr int16_t VICII_PIPELINE_DELAY_PIXELS = 12;
constexpr int16_t VICII_X_CENTERING_PIXELS = -14;
// C64 color palette - RGBA format
// Source: unusedino.de/ec64/technical/misc/vic656x/colors
static const uint32_t c64_palette_unusedino[16] = {
    0xFF000000, 0xFFFFFFFF, 0xFF2B3768, 0xFFB2A470,
    0xFF863D6F, 0xFF438D58, 0xFF792835, 0xFF6FC7B8,
    0xFF254F6F, 0xFF003943, 0xFF59679A, 0xFF444444,
    0xFF6C6C6C, 0xFF84D29A, 0xFFB55E6C, 0xFF959595
};

// Source: lospec.com/palette-list/commodore64
static const uint32_t c64_palette_lospec[16] = {
    0xFF000000, 0xFFFFFFFF, 0xFF444E9F, 0xFFCDBF6A,
    0xFFA357A0, 0xFF5EAB5C, 0xFF9B4550, 0xFF87D4C9,
    0xFF12546D, 0xFF3C68A1, 0xFF757ECB, 0xFF626262,
    0xFF898989, 0xFF9BE29A, 0xFFCD7E88, 0xFFADADAD
};

// Source: c64-wiki.com/wiki/Color
static const uint32_t c64_palette_c64wiki[16] = {
    0xFF000000, 0xFFFFFFFF, 0xFF000088, 0xFFEEFFAA,
    0xFFCC44CC, 0xFF55CC00, 0xFFAA0000, 0xFF77EEEE,
    0xFF5588DD, 0xFF004466, 0xFF7777FF, 0xFF333333,
    0xFF777777, 0xFF66FFAA, 0xFFFF8800, 0xFFBBBBBB
};

// Named palette registry — indexed by vicii_base_t::get_named_palettes()
static const NamedPalette c64_named_palettes[] = {
    { "unusedino", "Unusedino",  c64_palette_unusedino, 16 },
    { "lospec",    "Lospec",     c64_palette_lospec,    16 },
    { "c64wiki",   "C64 Wiki",   c64_palette_c64wiki,   16 },
};
static constexpr int c64_named_palette_count = 3;

// ========================================================================================
// INLINE UTILITY FUNCTIONS
// ========================================================================================

// Get default palette
const uint32_t* vicii_base_t::get_default_palette() {
    return c64_palette_unusedino;
}

// ========================================================================================
// BORDER LOGIC
// ========================================================================================

static inline void vicii_border_update_limits(vicii_border_unit_t* border, uint8_t c1_reg, uint8_t c2_reg) {
    border->border_top = (c1_reg & fld::C1_RSEL) ?
        VICII_BORDER_TOP_RSEL1 : VICII_BORDER_TOP_RSEL0;
    border->border_bottom = (c1_reg & fld::C1_RSEL) ?
        VICII_BORDER_BOTTOM_RSEL1 : VICII_BORDER_BOTTOM_RSEL0;
    border->border_left = (c2_reg & fld::C2_CSEL) ?
        VICII_BORDER_LEFT_CSEL1 : VICII_BORDER_LEFT_CSEL0;
    border->border_right = (c2_reg & fld::C2_CSEL) ?
        VICII_BORDER_RIGHT_CSEL1 : VICII_BORDER_RIGHT_CSEL0;
}

// Two-stage vertical border latch check (VICE: check_vborder_top/bottom).
// Called when any input changes: raster_counter advance or $D011 write.
// Inputs: raster_counter, DEN bit, border_top, border_bottom.
static inline void vicii_check_vertical_border(vicii_base_t* vicii) {
    const uint16_t raster = vicii->timing.raster_counter;
    const bool den_set = (vicii->regs_[reg::C1] & fld::C1_DEN) != 0;
    
    // check_vborder_top: top border + DEN → clear both immediately
    if (raster == vicii->border.border_top && den_set) {
        vicii->border.vertical_border_flip_flop = false;
        vicii->border.set_vertical_border_flip_flop = false;
    }
    
    // check_vborder_bottom: bottom border → set staged latch
    // (transferred to active flip-flop at left border position and start of line)
    if (raster == vicii->border.border_bottom) {
        vicii->border.set_vertical_border_flip_flop = true;
    }
}

// ========================================================================================
// PIXEL SEQUENCER AND GRAPHICS - HARDWARE-ACCURATE ARCHITECTURE
// ========================================================================================

// Update raster_flags_ and drive_flags_ after a raster counter change.
// VSync maps to vblank; Blank is set for the entire vblank region.
static inline void vicii_update_vblank_flags(vicii_base_t* vicii) {
    const uint16_t raster = vicii->timing.raster_counter;
    bool in_vblank;
    if (vicii->cached_first_vblank_line > vicii->cached_last_vblank_line) {
        in_vblank = (raster >= vicii->cached_first_vblank_line ||
                     raster <= vicii->cached_last_vblank_line);
    } else {
        in_vblank = (raster >= vicii->cached_first_vblank_line &&
                     raster <= vicii->cached_last_vblank_line);
    }
    vicii->raster_flags_ = in_vblank
        ? (SyncFlag::VSync | SyncFlag::Blank)
        : SyncFlag::None;

    // Replace the raster component of drive_flags_ while keeping horizontal bits
    constexpr SyncFlag raster_mask = SyncFlag::VSync | SyncFlag::Blank;
    vicii->drive_flags_ = vicii->raster_flags_
        | (vicii->drive_flags_ & ~raster_mask);
}

// CRITICAL INSIGHT: Real VIC-II hardware separation of concerns:
// 1. G-access cycles load graphics data into 8-bit shift register
// 2. Pixel sequencer reads from shift register and outputs exactly 8 pixels per cycle
// 3. Each cycle produces exactly 8 pixels, regardless of graphics mode
// 4. Multicolor modes consume 2 bits per pixel, standard modes consume 1 bit per pixel

// Helper function: Convert fetch X coordinate to display buffer position
// Applies hardware pipeline delay (12px) and visual centering adjustment
// Returns buffer position (0-402 for PAL) or -1 if not in visible range
static inline int16_t vicii_fetch_x_to_buffer_pos(const vicii_base_t* vicii, uint16_t fetch_x_coord) {
    // Apply hardware pipeline delay and visual centering adjustment.
    // Uses pre-computed session-constant values to avoid per-pixel modulo
    // and repeated pointer dereferences through vicii->config->.
    const uint16_t ppl = vicii->cached_pixels_per_line;

    // Conditional subtract replaces modulo on non-power-of-2 (504 PAL / 520 NTSC).
    // fetch_x_coord is in [0, ppl-1], offset is ppl-2, so sum is in [ppl-2, 2*ppl-3].
    uint16_t display_x_coord = fetch_x_coord + vicii->cached_display_offset;
    if (display_x_coord >= ppl) display_x_coord -= ppl;

    const uint16_t first_vis = vicii->cached_first_visible_display;
    const uint16_t vis_pixels = vicii->cached_visible_pixels;

    // Calculate position in line buffer (0-402 for PAL)
    // The visible range wraps around: [first_vis .. first_vis+vis_pixels)
    uint16_t buffer_pos;
    if (display_x_coord >= first_vis) {
        buffer_pos = display_x_coord - first_vis;
    } else if (display_x_coord < vicii->cached_wrap_threshold) {
        buffer_pos = (ppl - first_vis) + display_x_coord;
    } else {
        return -1;
    }

    if (buffer_pos < vis_pixels) {
        return buffer_pos;
    }
    return -1;
}

// X-coordinate driven pixel emission for precise positioning
// The line buffer contains pixels in display order (0-402 for PAL visible area).
// x_coordinate values wrap around (0-503 for PAL), so we must map them correctly.
static inline void vicii_pixel_emit_at_x(vicii_base_t* vicii, const vicii_pixel_t* pixel_data, uint16_t x_coord) {
    // Convert fetch position to buffer position (handles pipeline delay and centering)
    const int16_t pixel_line_x = vicii_fetch_x_to_buffer_pos(vicii, x_coord);
    
    if (pixel_line_x >= 0) {
        vicii->pixel.pixel_line_priority[pixel_line_x] = pixel_data->priority;
        vicii->pixel.color_line[pixel_line_x] = pixel_data->color;
    }
}

// ========================================================================================
// INTERRUPT HANDLING
// ========================================================================================

// Helper function: Set an interrupt and update IRQ flag
// This is used by all three interrupt sources:
// - VICII_IR_IRST (0x01): Raster interrupt
// - VICII_IR_IMBC (0x02): Sprite-sprite collision interrupt
// - VICII_IR_IMMC (0x04): Sprite-data collision interrupt
// - VICII_IR_ILP  (0x08): Light pen interrupt
//
// Documentation (vic-ii.txt lines 2244-2285):
// "If at least one latch bit and the belonging bit in the enable register is
// set, the IRQ line is held low and so the interrupt is triggered in the
// processor."
static inline void vicii_set_interrupt(vicii_base_t* vicii, uint8_t interrupt_mask) {
    // Set the interrupt latch bit(s)
    vicii->regs_[reg::IR] |= interrupt_mask;
    
    // Check if this interrupt is enabled and update IRQ flag
    const uint8_t latched_interrupts = vicii->regs_[reg::IR] & fld::IR_INTERRUPTS;
    const uint8_t enabled_interrupts = vicii->regs_[reg::IE] & fld::IR_INTERRUPTS;
    
    // Set IRQ flag if any enabled interrupt is latched
    if (latched_interrupts & enabled_interrupts) {
        vicii->regs_[reg::IR] |= fld::IR_IRQ;
    }
    
    // NOTE: IRQ line will be updated in vicii_tick() based on register state
    // We don't update it here to avoid side-effects on bus_state
}

// ========================================================================================
// SPRITE HANDLING
// ========================================================================================

// Get sprite X coordinate (9-bit value from registers)
// Maximally optimized: single calculation, no branching
static inline uint16_t vicii_sprite_get_x(const vicii_base_t* vicii, int sprite_num) {
    // Combine 8-bit base register with 9th bit from MX8 register
    // Uses bitwise operations for maximum performance
    return vicii->regs_[reg::M0X + sprite_num * 2] |
         ((vicii->regs_[reg::MX8] & (1 << sprite_num)) << (8 - sprite_num));
}

static inline void vicii_sprite_emit_pixels(vicii_base_t* vicii, int param_sprite_num) {
    vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[param_sprite_num];
    
    if (!(vicii->regs_[reg::MXE] & (1 << param_sprite_num)) || !sprite->display_state) return;
    
    const uint8_t sprite_bit = (1 << param_sprite_num);
    const uint16_t sprite_x = vicii_sprite_get_x(vicii, param_sprite_num);
    const uint16_t base_x = vicii->timing.x_coordinate;
    
    // Sprite width: 24 pixels standard, 48 pixels when X-expanded
    // Use cached x_expand/multicolor — updated on MXXE/MXMC register writes.
    const bool x_expanded = sprite->x_expand;
    const uint16_t sprite_width = x_expanded ? 48 : 24;
    
    // Quick range check: do any of the 8 pixels in this cycle overlap the sprite?
    if ((base_x + 7) < sprite_x || base_x >= (sprite_x + sprite_width)) return;
    
    const bool is_multicolor = sprite->multicolor;
    
    // Hoist color register reads before the 8-pixel loop to prevent
    // aliasing-induced reloads after writes to pixel/collision buffers.
    const uint8_t sprite_own_color = vicii->regs_[reg::M0C + param_sprite_num];
    const uint8_t mm0 = vicii->regs_[reg::MM0];
    const uint8_t mm1 = vicii->regs_[reg::MM1];
    
    // Process all 8 pixels in this cycle (matching the graphics sequencer)
    for (int pixel = 0; pixel < 8; pixel++) {
        const uint16_t current_x = base_x + (uint16_t)pixel;
        
        // Per-pixel range check within the sprite
        if (current_x < sprite_x || current_x >= (sprite_x + sprite_width)) continue;
        
        // Screen pixel offset within sprite, then map to shift register data index
        const uint8_t screen_pixel_x = (uint8_t)(current_x - sprite_x);
        const uint8_t data_pixel_x = x_expanded ? (screen_pixel_x >> 1) : screen_pixel_x;
        
        // Extract raw pixel data from shift register to determine transparency.
        uint8_t sprite_pixel_data;
        
        if (is_multicolor) {
            // Each 2-bit pair covers 2 adjacent data pixels (4 screen pixels when X-expanded)
            const uint8_t pair_index = data_pixel_x >> 1;  // 0-11
            sprite_pixel_data = (sprite->shift_reg >> (22 - pair_index * 2)) & 3;
        } else {
            // Single color: 1 bit per data pixel
            sprite_pixel_data = (sprite->shift_reg >> (23 - data_pixel_x)) & 1;
        }
        
        if (sprite_pixel_data == 0) continue;  // transparent in both modes
        
        // Convert to line buffer position (handles hardware pipeline delay and centering).
        const int16_t pixel_line_x = vicii_fetch_x_to_buffer_pos(vicii, current_x);
        if (pixel_line_x < 0 || pixel_line_x >= (int16_t)vicii->cached_visible_pixels) continue;
        
        // ---- Collision detection (independent of display priority) ----
        // Documentation (VIC-II-Updated2025.txt section 3.8.2):
        //   MxM: "two or more sprite data sequencers output a non-transparent pixel"
        //   MxD: "one or more sprite data sequencers output a non-transparent pixel
        //         and the graphics data sequencer outputs a foreground pixel"
        // Collision is based on raw sequencer output, NOT on what is displayed.
        // Collision does not need sprite_color — only sprite_bit and buffer indices.
        // Disabled when vertical border flip flop is set (section 3.9).
        if (!vicii->border.vertical_border_flip_flop) {
            const uint8_t existing_sprites = vicii->pixel.sprite_collision_line[pixel_line_x];
            
            // Sprite-sprite collision (MMC): any two sprites non-transparent at same position
            if (existing_sprites != 0) {
                // Set bits for ALL sprites involved in the collision (current + all existing)
                const uint8_t collision_mask = existing_sprites | sprite_bit;
                // Documentation (section 3.12): "only the first collision will trigger
                // an interrupt (i.e. if the collision register contained zero before the
                // collision)". The latch bit is always set; the IE register only gates IRQ.
                const bool was_zero = (vicii->regs_[reg::MXM_2] == 0);
                vicii->regs_[reg::MXM_2] |= collision_mask;
                if (was_zero) {
                    vicii_set_interrupt(vicii, fld::IR_IMMC);
                }
            }
            
            // Sprite-data collision (MBC): sprite non-transparent AND graphics foreground
            // Uses separate graphics_fg_line buffer that tracks raw graphics sequencer output,
            // independent of any sprite overwrites in the display priority buffer.
            if (vicii->pixel.graphics_fg_line[pixel_line_x]) {
                const bool was_zero = (vicii->regs_[reg::MXD_2] == 0);
                vicii->regs_[reg::MXD_2] |= sprite_bit;
                if (was_zero) {
                    vicii_set_interrupt(vicii, fld::IR_IMBC);
                }
            }
            
            // Record this sprite's presence at this pixel for future collision checks
            vicii->pixel.sprite_collision_line[pixel_line_x] |= sprite_bit;
        }
        
        // ---- Display priority (separate from collision detection) ----
        // Documentation (VIC-II-Updated2025.txt section 3.8.2):
        //   MxDP=0: sprite displays in front of foreground graphics
        //   MxDP=1: sprite displays behind foreground graphics
        // Between sprites: lower-numbered sprite ALWAYS has higher display priority.
        // Processing order 7→0 ensures lower-numbered sprites overwrite higher-numbered ones.
        const vicii_priority_t current_display_priority = vicii->pixel.pixel_line_priority[pixel_line_x];
        const vicii_priority_t sprite_priority = sprite->priority;
        // Sprite always overwrites a previous sprite (odd priority values: 1, 3)
        // due to 7→0 processing order. Otherwise, sprite wins if its priority
        // is strictly higher than the current pixel's.
        const bool sprite_wins_display =
            (current_display_priority & 1) | (sprite_priority > current_display_priority);
        
        if (sprite_wins_display) {
            vicii->pixel.pixel_line_priority[pixel_line_x] = sprite_priority;
            // Determine sprite color only when the pixel will actually be displayed.
            // Deferred past collision detection and priority check to avoid color
            // register reads when the sprite pixel is occluded.
            uint8_t sprite_color;
            if (is_multicolor) {
                switch (sprite_pixel_data) {
                    case 1:  sprite_color = mm0; break;
                    case 2:  sprite_color = sprite_own_color; break;
                    default: sprite_color = mm1; break;  // case 3
                }
            } else {
                sprite_color = sprite_own_color;
            }
            
            vicii->pixel.color_line[pixel_line_x] = sprite_color;
        }
    } // end 8-pixel loop
}

// Sprite sequencer — emit pixels for all enabled sprites with active display state.
// Early-out when no sprites are enabled (MXE == 0) saves ~8 function calls per cycle
// on sprite-free frames (the common case for many programs).
void vicii_sprite_sequencer(vicii_base_t* vicii) {
    if (vicii->regs_[reg::MXE] == 0) return;
    for (int i = VICII_NUM_SPRITES - 1; i >= 0; i--) {
        vicii_sprite_emit_pixels(vicii, i);
    }
}

// Pixel sequencer - sequences exactly 8 pixels per cycle
// This is the ONLY function that emits pixels to the framebuffer
static void vicii_pixel_sequencer(vicii_base_t* vicii) {
    // VICE-compatible deferred right border: Apply pending main_border=true from the
    // previous cycle's right border check. This 1-cycle deferral matches VICE's
    // border_state pipeline delay and gives the CPU time to change CSEL (via DEC $D016)
    // between the comparison check and the actual flip-flop set. Without this, the
    // CSEL side-border-opening trick fails because our pixel-level comparison at x=344
    // fires in the same VIC cycle as the CPU write, while VICE's cycle-level check at
    // cycle 57 fires one cycle later (giving the CPU's DEC time to take effect).
    if (vicii->border.deferred_right_border) {
        vicii->border.main_border_flip_flop = true;
        vicii->border.deferred_right_border = false;
    }
    
    // Use x_coordinate directly from the timing unit.
    // The VIC-II fetches graphics data at x_coordinate, but those pixels are displayed
    // 12 pixels later due to the pipeline delay. The vicii_pixel_emit_at_x() function
    // handles this delay by adding the hardware pipeline delay (12px) plus visual
    // centering adjustment when writing to the line buffer.
    //
    // This ensures CPU register writes (like border/background color changes) affect
    // pixels being OUTPUT at that moment, not pixels being LOADED into the pipeline.
    const uint16_t x_coord = vicii->timing.x_coordinate;
    
    // Border limits for per-pixel comparison
    const uint16_t border_left = vicii->border.border_left;
    const uint16_t border_right = vicii->border.border_right;
    
    // ---------------------------------------------------------------
    // FAST-PATH BORDER DETECTION
    // ---------------------------------------------------------------
    // Border transitions occur at exact pixel positions (border_left / border_right),
    // which fall WITHIN one specific cycle each. On >97% of cycles (61/63 for PAL),
    // no transition occurs and all 8 pixels share the same border state. We detect
    // this common case with two range checks and skip the per-pixel comparison loop.
    //
    // On the rare transition cycles (~2/63 per line), we fall through to the original
    // per-pixel logic that checks flip-flops at each pixel position for exact timing.
    //
    // WHY NOT CYCLE-LEVEL CALLBACKS: Border pixel positions (e.g., CSEL=1 left=24)
    // fall mid-cycle (pixel 4 of x_cycle 15 for PAL). Cycle-level callbacks fire at
    // cycle granularity and require a one-cycle pipeline delay (VICE: border_state)
    // to defer rendering. But our pixel emission uses X-coordinate positioning (not
    // sequential dbuf_offset like VICE), so the pipeline delay shifts the display
    // area by ~12 pixels, causing massive regressions. Per-pixel checks within the
    // transition cycle preserve exact sub-cycle timing.
    const bool has_left_transition  = (border_left  >= x_coord && border_left  <= x_coord + 7);
    const bool has_right_transition = (border_right >= x_coord && border_right <= x_coord + 7);
    
    // border_mask: bit N set → pixel N is in border area.
    // Used by both the border emission loop and the display pixel loop below.
    uint8_t border_mask;
    vicii_sequencer_unit_t* seq = &vicii->sequencer;
    
    if (!has_left_transition && !has_right_transition) {
        // FAST PATH (>97% of cycles): Uniform border state for all 8 pixels.
        // No flip-flop transitions occur in this span, so the current state applies
        // identically to all 8 pixels without per-pixel checks.
        // NOTE: Only main_border is checked for rendering, NOT vborder (matching VICE).
        // The vborder flag controls whether main_border is CLEARED at the left border
        // check (Rule 6), but is not a direct rendering input. This distinction is
        // critical for the CSEL side-border-opening trick, where main_border stays 0
        // (CSEL trick prevents right border set) even though vborder=1.
        const bool in_border = vicii->border.main_border_flip_flop;
        border_mask = in_border ? 0xFF : 0x00;
        
        if (in_border) {
            // Actual border area: emit border color with BORDER priority.
            // Batch-fill: compute base buffer position once instead of calling
            // vicii_fetch_x_to_buffer_pos 8 times through vicii_pixel_emit_at_x.
            // 8 consecutive X coords always map to 8 consecutive buffer positions
            // (no wrap within a single cycle).
            const int16_t base = vicii_fetch_x_to_buffer_pos(vicii, x_coord);
            if (base >= 0) {
                // Clamp count: visible_pixels_per_line (403 PAL) is not a multiple
                // of 8, so the last visible cycle may only have a partial span.
                const int16_t vp = vicii->cached_visible_pixels;
                const int count = (base + 8 <= vp) ? 8 : (vp - base);
                memset(&vicii->pixel.pixel_line_priority[base], vicii->border.border_pixel.priority, count);
                memset(&vicii->pixel.color_line[base], (uint8_t)vicii->border.border_pixel.color, count);
            }
        } else if (!vicii->video_logic.display_state) {
            // Content area in idle mode: emit background color with BACKGROUND priority.
            // This allows sprites to display over idle areas (sprites have higher priority
            // than BACKGROUND but lower than BORDER). The VIC-II displays idle pattern
            // graphics in this state, but we approximate with background color for now.
            const int16_t base = vicii_fetch_x_to_buffer_pos(vicii, x_coord);
            if (base >= 0) {
                const int16_t vp = vicii->cached_visible_pixels;
                const int count = (base + 8 <= vp) ? 8 : (vp - base);
                memset(&vicii->pixel.pixel_line_priority[base], VICII_PRIORITY_BACKGROUND, count);
                memset(&vicii->pixel.color_line[base], vicii->regs_[reg::B0C] & 0x0F, count);
            }
        }
    } else {
        // SLOW PATH (~3% of cycles): Border transition occurs in this 8-pixel span.
        // Check border flip-flops at EACH pixel position for exact sub-cycle timing.
        border_mask = 0;
        const uint16_t raster = vicii->timing.raster_counter;
        const uint16_t border_bottom = vicii->border.border_bottom;
        
        for (int pixel = 0; pixel < 8; pixel++) {
            const uint16_t pixel_x = x_coord + (uint16_t)pixel;
            
            // Rule 1: "If the X coordinate reaches the right comparison value,
            // the main border flip flop is set."
            if (pixel_x == border_right) {
                vicii->border.main_border_flip_flop = true;
            }
            
            // Rules 4, 5, 6: Handle left coordinate checks
            if (pixel_x == border_left) {
                // VICE-style two-stage vborder latch:
                // At the left border position, check bottom border condition and
                // transfer the staged vborder latch to the active flip-flop.
                
                // Rule 4 / check_vborder_bottom: set staged latch if raster matches bottom
                if (raster == border_bottom) {
                    vicii->border.set_vertical_border_flip_flop = true;
                }
                
                // Transfer staged latch → active flip-flop (VICE: vborder = set_vborder)
                vicii->border.vertical_border_flip_flop = vicii->border.set_vertical_border_flip_flop;
                
                // Rule 6: "If the X coordinate reaches the left comparison value and the vertical
                // border flip flop is not set, the main flip flop is reset."
                if (!vicii->border.vertical_border_flip_flop) {
                    // Detect main border opening transition for XSCROLL initialization.
                    if (vicii->border.main_border_flip_flop) {
                        seq->xscroll_counter = vicii->regs_[reg::C2] & fld::C2_XSCROLL;
                        seq->pixel_in_char = 0;
                        seq->display_vmli = 0;
                    }
                    vicii->border.main_border_flip_flop = false;
                }
            }
            
            // Determine if THIS specific pixel is border or display
            // Only main_border is checked (matching VICE: vborder is not a render input)
            const bool in_border = vicii->border.main_border_flip_flop;
            if (in_border) border_mask |= (1 << pixel);
            
            if (in_border) {
                vicii_pixel_emit_at_x(vicii, &vicii->border.border_pixel, pixel_x);
            } else if (!vicii->video_logic.display_state) {
                // Content area idle mode: background color with BACKGROUND priority
                vicii_pixel_t idle_pixel;
                idle_pixel.color = (vicii_color_t)(vicii->regs_[reg::B0C] & 0x0F);
                idle_pixel.priority = VICII_PRIORITY_BACKGROUND;
                vicii_pixel_emit_at_x(vicii, &idle_pixel, pixel_x);
            }
        }
    }
    
    // Now handle display area pixel sequencing
    
    // Only process display logic if we're in display state
    if (vicii->video_logic.display_state) {
        
        // Reset shift register on mode change (but not x-scroll)
        if (seq->graphics_mode != seq->last_mode) {
            seq->last_mode = seq->graphics_mode;
            seq->shift_reg = 0;
        }
        // LATCH-BASED SHIFT REGISTER MODEL
        // On real VIC-II hardware, each g-access fills an internal data latch.
        // The shift register reloads from the latch when the previous character's
        // 8 pixels have been fully consumed (pixel_in_char wraps to 0).  This means
        // character pixel output is NOT aligned to cycle boundaries — it spans
        // across cycles.  With first_x_coord=404 (PAL), the border opens at pixel 4
        // of cycle 15 (x=24).  Column 0's SR loads at that moment, outputting bits
        // 7-0 across pixels 4-7 of cycle 15 and pixels 0-3 of cycle 16.  This gives
        // all 40 columns exactly 8 visible pixels within the 320-pixel display window.
        //
        // The display_vmli counter tracks which column to load next (0-39),
        // independent of the g-access vmli.  The SR reload happens inside the pixel
        // loop when pixel_in_char == 0, driven by the display sequencer state.
        
        // Hoist background color reads to locals — prevents aliasing-induced reloads
        // after vicii_pixel_emit_at_x() writes through uint8_t* pointers.
        // Safe within a single cycle: pixel sequencer runs during PHI1; CPU color
        // register writes happen during PHI2, so B0C-B3C cannot change mid-cycle.
        const uint8_t bg0 = vicii->regs_[reg::B0C];
        const uint8_t bg1 = vicii->regs_[reg::B1C];
        const uint8_t bg2 = vicii->regs_[reg::B2C];
        const uint8_t bg3 = vicii->regs_[reg::B3C];
        
        // Sequence exactly 8 pixels from shift register
        for (int pixel = 0; pixel < 8; pixel++) {
            const uint16_t pixel_x = x_coord + (uint16_t)pixel;
            
            // Re-check border state for this pixel using the border_mask computed
            // in the first loop. We MUST NOT use the live flip-flop here because
            // the first loop may have cleared it mid-cycle (e.g., at border_left),
            // which would incorrectly make earlier pixels appear as display.
            const bool pixel_in_border = (border_mask & (1 << pixel)) != 0;
            
            // Skip if this pixel is in border (already handled in first loop).
            // The shift register is not advanced during border because the border
            // unit suppresses pixel output.  The SR will be loaded from the latch
            // when the border opens and pixel_in_char == 0, starting from bit 7.
            if (pixel_in_border) {
                continue;
            }
            
            vicii_pixel_t pixel_data;
            
            // XSCROLL handling - delay pixel output by XSCROLL pixels
            // Background color during scroll delay uses the hoisted bg0 local.
            if (seq->xscroll_counter > 0) {
                seq->xscroll_counter--;
                pixel_data.color = static_cast<vicii_color_t>(bg0);
                pixel_data.priority = VICII_PRIORITY_BACKGROUND;
                vicii_pixel_emit_at_x(vicii, &pixel_data, pixel_x);
                continue;
            }
            
            // Reload shift register when starting a new character.
            // On real VIC-II, the g-access data latch transfers to the SR when
            // the previous character's 8 pixels have been fully shifted out
            // (pixel_in_char wraps to 0).  This decouples character pixel timing
            // from cycle boundaries, allowing columns to span across cycles.
            if (seq->pixel_in_char == 0 && seq->display_vmli < VICII_CHARS_PER_LINE) {
                seq->shift_reg = seq->graphics_line[seq->display_vmli];
                seq->active_display_column = seq->display_vmli;
                seq->display_vmli++;
            }
            const uint8_t vmli = seq->active_display_column;
            
            // Extract pixel from shift register based on graphics mode
            uint8_t color_index = 0;
            uint8_t pixel_bits = 0;
            bool is_background = false;
            
            switch (seq->graphics_mode) {
                case VICII_GM_STANDARD_TEXT:
                    pixel_bits = (seq->shift_reg >> 7) & 1;
                    color_index = pixel_bits ?
                        (uint8_t)vicii->video_data.video_color_line[vmli] :
                        bg0;
                    is_background = (pixel_bits == 0);
                    seq->shift_reg <<= 1;
                    break;
                    
                case VICII_GM_MULTICOLOR_TEXT:
                    if (vicii->video_data.video_color_line[vmli] & 0x08) {
                        // Multicolor character - 2 bits per pixel, displayed double-width
                        // Each 2-bit pair spans 2 screen pixels. Shift on odd pixel_in_char
                        // to align MC pairs with the character pixel grid (not cycle position).
                        pixel_bits = (seq->shift_reg >> 6) & 3;
                        switch (pixel_bits) {
                            case 0: color_index = bg0; break;
                            case 1: color_index = bg1; break;
                            case 2: color_index = bg2; break;
                            case 3: color_index = vicii->video_data.video_color_line[vmli]; break;
                        }
                        is_background = (pixel_bits <= 1);  // MCM=1: "00","01" = background
                        if (seq->pixel_in_char & 1) {
                            seq->shift_reg <<= 2;
                        }
                    } else {
                        // Standard character in multicolor mode
                        pixel_bits = (seq->shift_reg >> 7) & 1;
                        color_index = pixel_bits ?
                            (uint8_t)vicii->video_data.video_color_line[vmli] :
                            bg0;
                        is_background = (pixel_bits == 0);
                        seq->shift_reg <<= 1;
                    }
                    break;
                    
                case VICII_GM_STANDARD_BITMAP:
                    pixel_bits = (seq->shift_reg >> 7) & 1;
                    if (pixel_bits) {
                        color_index = vicii->video_data.video_matrix_line[vmli] >> 4;
                    } else {
                        color_index = vicii->video_data.video_matrix_line[vmli] & 0x0F;
                    }
                    is_background = (pixel_bits == 0);
                    seq->shift_reg <<= 1;
                    break;
                    
                case VICII_GM_MULTICOLOR_BITMAP:
                    // Multicolor bitmap - 2 bits per pixel, displayed double-width
                    // Each 2-bit pair spans 2 screen pixels. Shift on odd pixel_in_char.
                    pixel_bits = (seq->shift_reg >> 6) & 3;
                    switch (pixel_bits) {
                        case 0: color_index = bg0; break;
                        case 1: color_index = vicii->video_data.video_matrix_line[vmli] >> 4; break;
                        case 2: color_index = vicii->video_data.video_matrix_line[vmli] & 0x0F; break;
                        case 3: color_index = vicii->video_data.video_color_line[vmli]; break;
                    }
                    is_background = (pixel_bits <= 1);  // MCM=1: "00","01" = background
                    if (seq->pixel_in_char & 1) {
                        seq->shift_reg <<= 2;
                    }
                    break;
                    
                case VICII_GM_ECM_TEXT:
                    pixel_bits = (seq->shift_reg >> 7) & 1;
                    if (pixel_bits) {
                        color_index = vicii->video_data.video_color_line[vmli];
                    } else {
                        const uint8_t bg_select = (vicii->video_data.video_matrix_line[vmli] >> 6) & 3;
                        const uint8_t bg_colors[4] = { bg0, bg1, bg2, bg3 };
                        color_index = bg_colors[bg_select];
                    }
                    is_background = (pixel_bits == 0);
                    seq->shift_reg <<= 1;
                    break;
                    
                default:
                    // Invalid modes (ECM+BMM, ECM+MCM, ECM+BMM+MCM):
                    // Documentation: "The VIC has 3 'invalid' text/bitmap modes that
                    // display only black pixels." Color output is always 0 (black)
                    // regardless of register settings. The shift register still runs
                    // (for sprite-data collision detection) but display is forced black.
                    color_index = 0;  // Always black
                    is_background = true;
                    // Still advance shift register to maintain sync
                    seq->shift_reg <<= 1;
                    break;
            }
            
            // Set pixel data and emit to line buffer.
            // Compute buffer position ONCE and reuse for both pixel emission and
            // collision detection — eliminates duplicate vicii_fetch_x_to_buffer_pos
            // call that was previously made separately for each foreground pixel.
            pixel_data.color = static_cast<vicii_color_t>(color_index);
            pixel_data.priority = is_background ? VICII_PRIORITY_BACKGROUND : VICII_PRIORITY_FOREGROUND;
            
            const int16_t buf_pos = vicii_fetch_x_to_buffer_pos(vicii, pixel_x);
            if (buf_pos >= 0) {
                vicii->pixel.pixel_line_priority[buf_pos] = pixel_data.priority;
                vicii->pixel.color_line[buf_pos] = pixel_data.color;
                
                // Track raw graphics foreground output for sprite-data collision
                // detection.  Independent of the display priority buffer — collisions
                // are based on the graphics data sequencer's raw output, not what's
                // displayed.
                // TODO: The graphics sequencer continues to run during left/right
                // border (main_border_flip_flop set, vertical not set), but currently
                // the pixel sequencer skips display processing for border pixels.
                // This means MxD collisions in the left/right border area are missed.
                // A future refactor should clock the shift register even during main
                // border for full accuracy.
                if (!is_background) {
                    vicii->pixel.graphics_fg_line[buf_pos] = true;
                }
            }
            
            // Increment pixel position within character (ALL modes).
            // This drives the SR reload timing: when pixel_in_char wraps to 0,
            // the next column's data is loaded from the latch into the SR.
            seq->pixel_in_char = (seq->pixel_in_char + 1) & 7;
        }
    }
    
    // Sprites are processed every cycle and can overlay any area
    // They have priority over both graphics and border pixels
    vicii_sprite_sequencer(vicii);

    // ---------------------------------------------------------------
    // PER-DOT-CLOCK STREAM DRIVING
    // ---------------------------------------------------------------
    // drive_flags_ was set for this cycle in vicii_set_x_cycle (once per cycle).
    // The 8-pixel loop reads it directly — zero per-pixel flag computation.
    // FrameEnd is a one-shot overlay consumed here.
    if (vicii->video_out_) {
        const uint16_t ppl = vicii->cached_pixels_per_line;
        const SyncFlag flags = vicii->drive_flags_;
        const bool is_vblank = has_flag(flags, SyncFlag::VSync);

        // FrameEnd: consume sticky flag set by line-0 cycle wrapper
        const bool frame_end = vicii->frame_wrapped_;
        if (frame_end) vicii->frame_wrapped_ = false;

        for (int pixel = 0; pixel < 8; pixel++) {
            uint16_t px = x_coord + static_cast<uint16_t>(pixel);
            if (px >= ppl) px -= ppl;

            // Resolved pixel color (0 if vblank or outside visible area)
            uint8_t color = 0;
            if (!is_vblank) {
                const int16_t buf_pos = vicii_fetch_x_to_buffer_pos(vicii, px);
                if (buf_pos >= 0)
                    color = vicii->pixel.color_line[buf_pos];
            }

            // FrameEnd on first pixel only
            SyncFlag pf = flags;
            if (frame_end && pixel == 0)
                pf = pf | SyncFlag::FrameEnd;

            vicii->video_out_->drive({color, pf});
        }
    }
}

// vicii_pixel_flush_line / vicii_raster_to_fb_row removed — per-dot-clock signal driving
// driving in vicii_pixel_sequencer replaces the collected-scanline flush path.

// ========================================================================================
// LIGHTPEN
// ========================================================================================

// Set the LP pin state. The VIC-II detects a negative edge (HIGH→LOW transition)
// and latches the current raster position into LPX/LPY registers.
// Only one negative edge is recognized per frame — subsequent edges are ignored
// until the next vertical blanking interval resets the latch.
void vicii_lightpen_set_pin(vicii_base_t* vicii, bool pin_high) {
    // Detect negative edge: previous HIGH, now LOW
    if (vicii->lightpen.lp_pin_prev && !pin_high) {
        if (!vicii->lightpen.triggered) {
            vicii->lightpen.triggered = true;
            // Latch current raster position
            vicii->regs_[reg::LPX] = (uint8_t)(vicii->timing.x_coordinate >> 1);
            vicii->regs_[reg::LPY] = (uint8_t)vicii->timing.raster_counter;
            // Signal the lightpen interrupt
            vicii_set_interrupt(vicii, fld::IR_ILP);
        }
    }
    vicii->lightpen.lp_pin_prev = pin_high;
}

// Accessors for external peripherals that need to compare their target
// position against the current raster beam position.
uint16_t vicii_base_t::get_raster_counter() const {
    return timing.raster_counter;
}

uint16_t vicii_base_t::get_x_coordinate() const {
    return timing.x_coordinate;
}

// ========================================================================================
// MEMORY ACCESS
// ========================================================================================

// Update memory mapping (Documentation section 2.4.2)
static inline void vicii_memory_update_mapping(vicii_memory_unit_t* memory, uint8_t mp_reg) {
    // Update addresses with bit operations
    // "VM10-VM13 (register $d018) that specify one of four 1KB blocks within the 16KB address space"
    memory->vm_base = ((uint16_t)mp_reg & 0xF0) << 6;  // VM10-VM13 bits * 0x400 -> << 6
    // "CB11-CB13 (register $d018) that specify one of eight 2KB blocks within the 16KB address space"
    memory->cb_base = ((uint16_t)mp_reg & 0x0E) << 10; // CB11-CB13 bits * 0x800 -> << 10
}

void vicii_base_t::memory_bank_change(void* chip, uint8_t bank) {
    vicii_base_t* vicii = (vicii_base_t*)chip;
    uint8_t inverted_bank = 3 - (bank & 0x03);  // Invert bank
    
    // Set the bank base offset (0x0000, 0x4000, 0x8000, or 0xC000)
    // This offset is applied to all VIC-II addresses, automatically selecting
    // the correct pre-calculated PLA banks with the proper #VA14 state baked in.
    //
    // When VIC-II reads address 0x1000 with bank_base=0x4000, it reads from
    // bus address 0x5000, which uses the pre-calculated mapping for bank 5
    // (with #VA14=1, showing RAM instead of CHARROM).
    vicii->memory.bank_base = inverted_bank * 0x4000;
}

static inline bus_state_t vicii_bus_memory_setup(vicii_base_t* vicii, bus_state_t bus_state, uint16_t address) {
    // Bank base offset applied here to keep operations in most appropriate place
    const uint16_t final_address = vicii->memory.bank_base | address;

    // Set up the address on the bus for the memory service phase to handle
    // This follows the same pattern as the CPU's bus_setup_read()
    BUS_SET_ADDR(bus_state, final_address);
    // RW line is guaranteed to be set (read mode) by the system tick:
    // each cycle starts with default_state (RW=1) and RW is re-set after
    // CPU PHI1 completes, so VIC-II never sees RW=0 here.
    return bus_state;
}

// ========================================================================================
// SEQUENCER LOGIC
// ========================================================================================

// vicii_sequencer_update_colors() removed — colors[] was write-only (never read in pixel loop)

// ========================================================================================
// REGISTER HANDLING
// ========================================================================================

static inline void vicii_sequencer_update_mode(vicii_sequencer_unit_t* sequencer, uint8_t c1_reg, uint8_t c2_reg) {
    sequencer->graphics_mode = ((c1_reg & (fld::C1_ECM | fld::C1_BMM)) |
                               (c2_reg & fld::C2_MCM)) >> 4;
}

// Timing functions
void vicii_update_badline_condition(vicii_base_t* vicii) {
    uint16_t raster = vicii->timing.raster_counter;
    
    // Bad lines only occur in range $30-$F6 (48-246) INCLUSIVE
    // VICE reference: VICII_LAST_DMA_LINE = $F7 (247), but allow_bad_lines is cleared
    // at the START of line $F7, so no bad lines actually occur on $F7 itself.
    // Effective range: $30-$F6 (198 values: 246-48 = 198).
    //
    // Optimized single comparison using intentional unsigned underflow:
    // When raster < 48, (raster - 48) underflows to large positive, making comparison false
    // When raster >= 48 && raster <= 246, (raster - 48) is in range [0, 198]
    uint16_t range_check = raster - 48;
    bool in_range = (range_check <= 198);
    
    if (in_range) {  // Equivalent to raster >= 48 && raster <= 247
        // "A Bad Line Condition is given at any arbitrary clock cycle, if at the
        // negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
        // <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
        // DEN bit was set during an arbitrary cycle of raster line $30."
        
        // Check if raster line $30 to capture DEN state
        if (raster == 0x30) {
            if (!vicii->video_logic.was_den_set_during_raster_30) {
                vicii->video_logic.was_den_set_during_raster_30 =
                    (vicii->regs_[reg::C1] & fld::C1_DEN) != 0;
            }
        }
        
        vicii->video_logic.is_bad_line = vicii->video_logic.was_den_set_during_raster_30 &&
                                 (static_cast<uint32_t>(raster & 0x07) == (vicii->regs_[reg::C1] & fld::C1_YSCROLL));
        
        // CRITICAL: The VIC-II latches display_state to true IMMEDIATELY when a bad line
        // condition is detected, at ANY cycle — not just at cycle 58.
        // Documentation: "The transition from idle to display state is triggered by the
        // Bad Line Condition" (Christian Bauer's VIC-II article).
        // VICE reference: viciisc/vicii-cycle.c check_badline() sets display_state=1
        // as soon as the bad line condition becomes true.
        // Cycle 58 only handles the reverse transition (display→idle when RC==7).
        if (vicii->video_logic.is_bad_line) {
            vicii->video_logic.display_state = true;
            // Latch: once a bad line condition is detected on any cycle of this
            // raster line, the c-access sequence will proceed to completion.
            // This prevents a subsequent CPU D011 write from cancelling an
            // already-committed steal sequence (required for FLI technique).
            vicii->video_logic.bad_line_occurred = true;
        }
    } else {
        vicii->video_logic.is_bad_line = false;
    }
}

// Helper function: Get 9-bit raster compare value from registers
// Bits 0-7 from $d012, bit 8 from $d011 bit 7
static inline uint16_t vicii_get_raster_compare(const vicii_base_t* vicii) {
    return (vicii->regs_[reg::RASTER] & 0xFF) |
           ((vicii->regs_[reg::C1] & fld::C1_RST8) ? 0x100 : 0);
}

static inline void vicii_registers_write_interrupt(uint8_t* regs, uint8_t value) {
    // Only consider the 4 actually supported interrupt bits (IRST/IMBC/IMMC/ILP)
    value &= fld::IR_INTERRUPTS;
    // Fetch the current Interrupt Register value
    uint8_t ir = regs[reg::IR];
    // Clear all '1' bits in the Interrupt Register that were written as '1'
    // Writing 1 to an interrupt bit acknowledges (clears) that interrupt
    ir &= ~value;
    
    // CRITICAL FIX: Recalculate IRQ flag (bit 7) after clearing interrupt latches
    // Documentation (vic-ii.txt lines 2284-2285):
    // "The bit 7 in the latch $d019 reflects the inverted state of the IRQ output of the VIC."
    // The IRQ line is held low when ANY enabled interrupt is latched.
    const uint8_t latched_interrupts = ir & fld::IR_INTERRUPTS;
    const uint8_t enabled_interrupts = regs[reg::IE] & fld::IR_INTERRUPTS;
    
    // Set IRQ flag if any enabled interrupt remains latched
    if (latched_interrupts & enabled_interrupts) {
        ir |= fld::IR_IRQ;
    } else {
        ir &= ~fld::IR_IRQ;  // Clear IRQ flag - all interrupts acknowledged
    }
    
    // Store the resulting bits (no need to set unused bits - they're only for reads)
    regs[reg::IR] = ir;
    
    // NOTE: IRQ line will be updated in vicii_tick() based on register state
    // We don't update it here to avoid side-effects on bus_state
}

// Register write function (uses all the above handlers)
bus_state_t vicii_base_t::registers_write(void* context, bus_state_t bus_state) {
    vicii_base_t* vicii = (vicii_base_t*)context;
    uint8_t value = BUS_GET_DATA(bus_state);
    uint8_t reg = BUS_GET_ADDR(bus_state) & reg::MASK; // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff

    // Notes:
    // * Some not-connected bits (marked with '-') are written anyway here,
    //   because determing the mask for those would only be slower, for no benefit
    //   (and these not-connected bits are turned into 1's in MaskBusRead anyway).
    // * Writes on 4 bit color registers ARE masked, to avoid having to do that in (often repeated) reads
    // * Instead of skipping writes to MxM and MxD, their reads are rerouted to MxM_2 and MxD_2
    // * Unused register indices 47..63 are written anyway here
    //   because avoiding those would only be slower, for no benefit

    if (reg == reg::IR) { // $d019 Interrupt Register
        // Treat the latching Interrupt Register differently from the other registers
        vicii_registers_write_interrupt(vicii->regs_.data, value);
        return bus_state; // No further processing needed for IR
    }
    
    // Mask color registers to 4 bits
    if (reg >= reg::EC) { // $d020 (4 bits) Exterior color (Border)
        // "When writing a color register ($D020-$D02E) currently being used to
        // display graphics a grey dot (color 15) appears at the first pixel of the
        // cycle. The reason for the grey dot appears to be a glitch in the color register
        // bank itself, not in the mapping from color enables to actual 4-bit color.
        // This effect is thus independent of the previous color register displayed."
        // Note: Grey dot effect only on 8565+ chips, implementation would require
        // cycle-exact tracking of which color registers are currently being displayed
        
        value &= 0x0F; // $d020 and up are colors - keep only lowest 4 bits
    }
    
    vicii->regs_[reg] = value;
    
    // Unit-specific update handlers
    switch (reg) {
        case reg::C1: // $d011 Control register 1
            // Update bad line condition when C1 changes (YSCROLL or DEN bit changes)
            vicii_update_badline_condition(vicii);
            // Update prev_raster_compare for edge detection (bit 8 changed)
            vicii->timing.prev_raster_compare = vicii_get_raster_compare(vicii);
            // Re-evaluate vertical border after DEN/RSEL change
            // (must happen after border_update_limits updates border_top/bottom)
            // Note: only reg::C1 affects vborder inputs (DEN, RSEL → border_top/bottom).
            // reg::C2 only changes CSEL (horizontal borders), so no vborder check needed.
            FALLTHROUGH; // to C2 case
        case reg::C2: // $d016 Control register 2
            vicii_sequencer_update_mode(&vicii->sequencer, vicii->regs_[reg::C1], vicii->regs_[reg::C2]);
            vicii_border_update_limits(&vicii->border, vicii->regs_[reg::C1], vicii->regs_[reg::C2]);
            if (reg == reg::C1) {
                vicii_check_vertical_border(vicii);
            }
            break;
        case reg::RASTER: // $d012 Raster compare (bits 0-7)
            // Update prev_raster_compare for edge detection
            vicii->timing.prev_raster_compare = vicii_get_raster_compare(vicii);
            break;
        case reg::MXE: // $d015 Sprite enabled x
            break;
        case reg::MXYE: // $d017 Sprite Y expansion
            // VICE reference (viciisc/vicii-mem.c d017_store):
            // Writing to MxYE does NOT directly set/clear the expansion flip-flop.
            // The only interaction: if a bit is cleared (Y-expand off) AND the sprite's
            // exp_flop is already 0, this triggers the "sprite crunch" effect and
            // resets exp_flop to 1. The flip-flop is otherwise only controlled by:
            //   - turn_sprite_dma_on: sets exp_flop = 1
            //   - check_exp at cycle 56: toggles exp_flop when DMA && Y-expanded
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
                uint8_t bit = (1 << i);
                if (!(value & bit) && !sprite->expansion_flip_flop) {
                    // Record that this sprite needs crunch processing.
                    // The MC/MCBASE corruption ("sprite crunch") is applied by the
                    // cycle 15 callback wrapper, which checks pending_mxye_crunch
                    // after the CPU PHI2 write completes.
                    vicii->sprites.pending_mxye_crunch |= bit;
                    sprite->expansion_flip_flop = true;
                }
            }
            break;
        case reg::MP: // $d018 Memory pointers
            vicii_memory_update_mapping(&vicii->memory, value);
            break;
        case reg::MXDP: // $d01b Sprite data priority
            // Batch update sprite priorities
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
                sprite->priority = (value & (1 << i)) ? 
                    VICII_PRIORITY_SPRITE_BEHIND : VICII_PRIORITY_SPRITE_IN_FRONT;
            }
            break;
        case reg::MXMC: // $d01c Sprite multicolor x select
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii->sprites.sprites[i].multicolor = (value & (1 << i)) != 0;
            }
            break;
        case reg::MXXE: // $d01d Sprite X expansion x
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii->sprites.sprites[i].x_expand = (value & (1 << i)) != 0;
            }
            break;
        case reg::EC: // $d020 (4 bits) Exterior color (Border)
            vicii->border.border_pixel.color = static_cast<vicii_color_t>(value); // value already masked to 0x0F above
            FALLTHROUGH; // to B0C-B2C case
        case reg::B0C: // $d021 (4 bits) Background color 0
        case reg::B1C: // $d022 (4 bits) Background color 1
        case reg::B2C: // $d023 (4 bits) Background color 2
            break;
        // MM0/MM1 ($d025/$d026) are global sprite multicolor registers.
        // No per-sprite decode needed — pixel sequencer reads the register value directly.
        // Fall through to default (register file already updated above).
        default:
            // No special handling needed for other registers
            break;
    }
    return bus_state;
}

// Compile-time bitmask of registers needing special handling
#define VICII_SPECIAL_REGS ( \
    (1ULL << reg::C1)  | (1ULL << reg::RASTER) | \
    (1ULL << reg::MXM) | (1ULL << reg::MXD)    | \
    (1ULL << reg::C2)  | (1ULL << reg::MP)     | \
    (1ULL << reg::IR)  | (1ULL << reg::IE)       \
)

bus_state_t vicii_base_t::registers_read(void* context, bus_state_t bus_state) {
    vicii_base_t* vicii = (vicii_base_t*)context;
    uint8_t reg = BUS_GET_ADDR(bus_state) & reg::MASK;
    uint8_t bus_data = BUS_GET_DATA(bus_state);
    uint8_t reg_val = vicii->regs_[reg];

    // Single test — one branch, predicted not-taken
    if (unlikely((VICII_SPECIAL_REGS >> reg) & 1)) {
        switch (reg) {
            case reg::C1:     reg_val = (reg_val & 0x7F) | ((vicii->timing.raster_counter >> 1) & fld::C1_RST8); break;
            case reg::RASTER: reg_val = vicii->timing.raster_counter & 0xFF; break;
            case reg::MXM:    reg_val = vicii->regs_[reg::MXM_2];
                               vicii->regs_[reg::MXM_2] = 0; break;
            case reg::MXD:    reg_val = vicii->regs_[reg::MXD_2];
                               vicii->regs_[reg::MXD_2] = 0; break;
            case reg::C2:     reg_val = bitmix(reg_val, bus_data, static_cast<uint8_t>(~fld::C2_UNUSED & 0xFF)); break;
            case reg::MP:     reg_val = bitmix(reg_val, bus_data, static_cast<uint8_t>(~fld::MP_UNUSED & 0xFF)); break;
            case reg::IR:     reg_val = bitmix(reg_val, bus_data, static_cast<uint8_t>(~fld::IR_UNUSED & 0xFF)); break;
            case reg::IE:     reg_val = bitmix(reg_val, bus_data, static_cast<uint8_t>(~fld::IE_UNUSED & 0xFF)); break;
        }
    }

    uint8_t mask = (reg < 47) * 0x0F | (reg < 32) * 0xF0;
    BUS_SET_DATA(bus_state, bitmix(reg_val, bus_data, mask));
    return bus_state;
}

// ========================================================================================
// TIMING AND VIDEO LOGIC
// ========================================================================================

void vicii_set_x_cycle(vicii_base_t* vicii, uint8_t value) {
    vicii->timing.x_cycle = value;
    
    // Update X coordinate (sprite/lightpen coordinate system)
    // X coordinate advances by 8 pixels per cycle (8 pixels displayed per cycle)
    // Documentation: "X coordinate 0" occurs halfway between cycles 13 and 14
    // The x_coordinate wraps at pixels_per_line (504 for PAL, 520 for NTSC)
    // Formula derived from vic-ii.txt timing diagram (lines 989-992):
    // - Cycle 13 start: x_coordinate = 0x1F4 (500)
    // - Cycle 14 start: x_coordinate = 0x004 (4)
    //
    // Uses conditional subtract instead of modulo (non-power-of-2):
    // raw = first_x_coord + cycle*8.  Max value (PAL): 404 + 62*8 = 900.
    // 900 < 2*504=1008, so at most one subtraction is needed.
    const uint16_t ppl = vicii->cached_pixels_per_line;
    uint16_t raw = vicii->cached_first_x_coord + (static_cast<uint16_t>(value) << 3);
    if (raw >= ppl) raw -= ppl;
    vicii->timing.x_coordinate = raw;
}

// Helper function: Reset VCBASE/VC when outside display area
// Called both when entering line 0 and throughout lines outside $30-$F7
static inline void vicii_reset_vcbase_vc(vicii_base_t* vicii) {
    vicii->video_logic.vcbase = 0;
    vicii->video_logic.vc = 0;
}

// Helper function: Check and trigger raster interrupt (edge-triggered)
// Documentation (vic-ii.txt lines 2262-2269):
// "Raster comparison is edge-triggered, not level-triggered. If $d012 is
// continuously updated to follow the raster counter, it will never trigger
// an IRQ condition. This edge-triggered behavior is documented in patent US4572506."
//
// This function is called ONCE at the start of each raster line (during line
// transition in vicii_timing_advance).  Because the raster counter only changes
// once per line, calling it at the transition point is inherently edge-triggered:
// the interrupt fires once when the raster first reaches the compare value,
// and won't fire again until the raster wraps around to that line in the next frame.
//
// CPU writes to $D012/$D011 that change the compare value to match the CURRENT
// raster line will NOT trigger an interrupt until the next natural line transition
// (patent US4572506 edge-triggered behavior).
static inline void vicii_check_raster_interrupt(vicii_base_t* vicii) {
    const uint16_t compare = vicii_get_raster_compare(vicii);
    
    if (compare == vicii->timing.raster_counter) {
        // Raster counter matches compare value — set the interrupt latch bit.
        // vicii_set_interrupt only asserts IRQ if the ERST enable bit is also set.
        vicii_set_interrupt(vicii, fld::IR_IRST);
    }
}

// Helper function: Perform line 0 raster/IRQ operations
// Documentation (vic-ii.txt lines 1006-1010, 1012-1014):
// "Raster line 0 is, however, an exception: In this line, IRQ and incrementing
// (resp. resetting) of RASTER are performed one cycle later than in the other lines."
//
// This function handles both immediate (NTSC) and delayed (PAL) execution
static inline void vicii_perform_line0_raster_irq_operations(vicii_base_t* vicii) {
    // Reset raster counter to 0
    vicii->timing.raster_counter = 0;

    // Update maintained drive_flags_ for raster 0's vblank state
    vicii_update_vblank_flags(vicii);
    
    // Reset per-frame state
    vicii->video_logic.was_den_set_during_raster_30 = false;
    vicii->video_logic.is_bad_line = false;
    vicii->video_logic.bad_line_occurred = false;
    vicii->video_logic.refresh_counter = 0xFF;

    // Reset lightpen trigger — can re-trigger on the new frame
    vicii->lightpen.triggered = false;
    
    // Reset VCBASE/VC (shared with timing advance logic)
    vicii_reset_vcbase_vc(vicii);
    
    // Check raster interrupt (edge-triggered)
    vicii_check_raster_interrupt(vicii);
}

// Helper: Reset line buffers for a new scanline
// Initializes pixel/priority/collision buffers to default border state
static inline void vicii_line_buffer_reset(vicii_base_t* vicii) {
    if (!vicii->pixel.color_line) return;
    
    const uint16_t width = vicii->cached_visible_pixels;
    const uint8_t border_color = vicii->regs_[reg::EC] & 0x0F;
    
    memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, width);
    memset(vicii->pixel.color_line, border_color, width);
    // Clear collision detection buffers for the new line
    memset(vicii->pixel.sprite_collision_line, 0, width);
    memset(vicii->pixel.graphics_fg_line, 0, width * sizeof(bool));
}

void vicii_timing_advance(vicii_base_t* vicii) {
    // Common case: advance within the current line
    if (vicii->timing.x_cycle < vicii->cached_cycles_per_line - 1) {
        vicii_set_x_cycle(vicii, vicii->timing.x_cycle + 1);
        return;
    }
    
    // --- End of line: advance raster, reset buffers ---
    // (Stream driving has moved to per-dot-clock emission in vicii_pixel_sequencer)
    
    const uint16_t completed_raster = vicii->timing.raster_counter;
    vicii_set_x_cycle(vicii, 0);
    
    // Reset bad line latch at the start of each new raster line.
    // The latch tracks whether a bad line condition was detected at ANY cycle
    // during the current raster line. It must be cleared when the raster counter
    // advances so the next line starts fresh.
    vicii->video_logic.bad_line_occurred = false;
    
    // VICE reference: No special handling at raster $2F. vcbase/vc are reset at
    // start-of-frame (line 0) in vicii_perform_line0_raster_irq_operations().
    // display_state is controlled by bad line conditions (set true) and cycle 58
    // RC==7 check (set false). No explicit reset needed here.
    
    // Advance raster counter
    // Documentation (vic-ii.txt lines 1012-1014):
    // "Note: After the end of raster line 311 in the 6569, the start of frame
    // (line 0) occurs one cycle late. Line timing wraps normally at cycle 63,
    // but the transition from line 311 to line 0 introduces this one-cycle delay."
    //
    // FRAME WRAP HANDLING:
    // When raster_counter would wrap (311→0 for PAL, 261→0 for NTSC), we DON'T
    // immediately reset to 0 here. Instead, we clamp at total_lines-1, and the
    // cycle wrappers handle the actual transition in the next cycle:
    // - PAL (6569): Cycle 1 wrapper executes line 0 ops (one-cycle delay)
    // - NTSC (6567): Cycle 0 wrapper executes line 0 ops (immediate)
    const uint16_t new_raster = completed_raster + 1;
    vicii->timing.raster_counter = (new_raster < vicii->cached_total_lines)
        ? new_raster
        : vicii->cached_total_lines - 1;  // Clamp; cycle wrappers reset to 0

    // Update maintained drive_flags_ for the new raster's vblank state
    vicii_update_vblank_flags(vicii);

    // Emit FrameEnd when raster reaches the centering start line.
    // Decoupled from line-0 operations so the signal frame can start
    // at an optimal raster for vertically centered display.
    if (vicii->timing.raster_counter == vicii->signal_frame_start_raster_)
        vicii->frame_wrapped_ = true;
    
    // Check raster interrupt on every line transition.
    // Line 0 raster interrupt is handled separately in
    // vicii_perform_line0_raster_irq_operations with proper PAL/NTSC delay timing.
    // Guard: after the increment above, raster_counter is always in [1, total_lines-1],
    // so this condition is always true — kept as a defensive invariant.
    if (vicii->timing.raster_counter != 0) {
        vicii_check_raster_interrupt(vicii);
    }
    
    // Prepare line buffers for the next scanline
    vicii_line_buffer_reset(vicii);
    
    // Vertical border check after raster_counter changed.
    // Also transfers staged vborder latch → active flip-flop at start of line
    // (VICE: vicii-cycle.c line 481).
    vicii_check_vertical_border(vicii);
    vicii->border.vertical_border_flip_flop = vicii->border.set_vertical_border_flip_flop;
}

// ========================================================================================
// CYCLE FUNCTIONS
// ========================================================================================

static uint8_t vicii_cycle_sprite_p_access(vicii_base_t* vicii, int param_sprite_num) {
    // VICE reference: P-access (pointer fetch) is ALWAYS unconditional.
    // The VIC-II always fetches the sprite pointer during PHI1, regardless of
    // DEN, $D015 enable, or DMA state. This is confirmed by VICE viciisc/vicii-fetch.c
    // vicii_fetch_sprite_pointer() which has no conditions whatsoever.
    vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[param_sprite_num];
    vicii->bus.active_sprite = sprite;
    return VIC_ACCESS_P;
}

static uint8_t vicii_cycle_sprite_s_access(vicii_base_t* vicii, int param_sprite_num) {
    // VICE reference: S-access always happens as a bus cycle (VIC-II always takes the bus).
    // The actual data fetch is gated on sprite_dma (dma_enabled) in the bus read logic,
    // but the bus cycle itself always occurs. Active_sprite is always set.
    vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[param_sprite_num];
    vicii->bus.active_sprite = sprite;
    return VIC_ACCESS_S;
}

static uint8_t vicii_cycle_refresh(vicii_base_t* vicii, int unused_param) {
    // VICE reference: Refresh access is UNCONDITIONAL — no DEN check.
    // The VIC-II always performs refresh cycles regardless of display enable state.
    // DEN only affects allow_bad_lines (captured at raster $30), which gates bad line
    // detection in vicii_update_badline_condition(). Separate DEN checks here were wrong.
    return VIC_ACCESS_REFRESH;
}

// Spec cycle 14 (x_cycle 13): VC update — refresh PHI1, VC/VMLI/RC update
//
// VICE reference (vicii-chip-model.c): Cycle 14 Phi2 has UpdateVc flag.
// Spec (vic-ii.txt line 1236): "In the first phase of cycle 14 of each line,
// VC is loaded from VCBASE (VCBASE->VC) and VMLI is cleared. If there is a
// Bad Line Condition in this phase, RC is also reset to zero."
//
// CRITICAL: VC load and VMLI clear are UNCONDITIONAL — they happen every line.
// This ensures each raster line within a character row re-reads from the same
// VCBASE position, with only RC differentiating which row of the character is shown.
static uint8_t vicii_cycle_refresh_vc_update(vicii_base_t* vicii, int unused_param) {
    vicii->video_logic.vc = vicii->video_logic.vcbase;
    vicii->video_logic.vmli = 0;
    
    // Only on bad lines: reset RC to zero (starts a new 8-row character block)
    // Documentation (vic-ii.txt line 1236-1238): "If there is a Bad Line Condition
    // in this phase, RC is also reset to zero."
    // VICE reference: Uses instantaneous bad_line check (not latched). If CPU writes
    // $D011 to change YSCROLL mid-line, the bad line condition can toggle on/off.
    // RC is only reset if the condition is true RIGHT NOW at cycle 14.
    if (vicii->video_logic.is_bad_line) {
        vicii->video_logic.rc = 0;
    }

    return VIC_ACCESS_REFRESH;
}

// Spec cycle 15 (x_cycle 14): First c-access — refresh PHI1, c-access PHI2
//
// VICE reference (vicii-chip-model.c): Cycle 15 has PHI1=Refresh, PHI2=FetchC.
// This is the first c-access (column 0). The VC update already happened in the
// previous cycle (spec 14 / x_cycle 13), so vmli=0 and vc=vcbase are ready.
//
// Returns VIC_ACCESS_REFRESH_C on bad lines (refresh PHI1 + c-access PHI2),
// or VIC_ACCESS_REFRESH on non-bad lines (refresh only).
static uint8_t vicii_cycle_refresh_first_c_access(vicii_base_t* vicii, int unused_param) {
    // VICE reference: Uses instantaneous bad_line check. If CPU writes $D011
    // clearing the bad line condition, c-access stops (used in FLI techniques).
    if (vicii->video_logic.is_bad_line) {
        return VIC_ACCESS_REFRESH_C;
    }
    return VIC_ACCESS_REFRESH;
}

// Process pending sprite crunch effects from $D017 writes during spec cycle 15 PHI2.
// VICE reference (viciisc/vicii-mem.c d017_store, viciisc/vicii-cycle.c):
// When the CPU clears a Y-expansion bit in $D017 during the second phase of
// cycle 15 while the expansion flip-flop is 0, the MC/MCBASE values are
// corrupted via the "sprite crunch" formula.
// This is processed here (in the cycle callback, after PHI2) rather than
// inline in the register write handler, keeping timing-dependent logic in the
// cycle callbacks where it belongs.
static inline void vicii_sprite_process_pending_crunch(vicii_base_t* vicii) {
    uint8_t crunch_mask = vicii->sprites.pending_mxye_crunch;
    if (!crunch_mask) return;
    
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        if (crunch_mask & (1 << i)) {
            vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
            uint8_t mc = sprite->mc;
            uint8_t mcbase = sprite->mcbase;
            // VICE: mc = (0x2a & (mcbase & mc)) | (0x15 & (mcbase | mc))
            sprite->mc = (0x2A & (mcbase & mc)) | (0x15 & (mcbase | mc));
            // mcbase is set from mc on the following cycle's mcbase_update
        }
    }
    vicii->sprites.pending_mxye_crunch = 0;
}

// Spec cycle 15 wrapper: First c-access + sprite crunch processing
// VICE reference (vicii-chip-model.c): Cycle 15 Phi2 has ChkSprCrunch flag.
static uint8_t vicii_cycle_refresh_first_c_access_sprite_crunch(vicii_base_t* vicii, int param) {
    uint8_t result = vicii_cycle_refresh_first_c_access(vicii, param);
    vicii_sprite_process_pending_crunch(vicii);
    return result;
}

// VICE reference (viciisc/vicii-cycle.c): sprite_mcbase_update() at cycle 16 Phi2
// MCBASE ← MC (if exp_flop set), DMA off if mcbase==63.
// CRITICAL for sprite multiplexing: DMA turns off early (cycle 16) so cycles 55/56
// can see !dma_enabled and re-trigger sprites on the same line.
static inline void vicii_sprite_mcbase_update(vicii_base_t* vicii) {
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
        if (sprite->expansion_flip_flop) {
            sprite->mcbase = sprite->mc;  // MCBASE ← MC
            if (sprite->mcbase == VICII_SPRITE_MC_MAX) {
                sprite->dma_enabled = false;  // DMA off when all data consumed
            }
        }
    }
}

static uint8_t vicii_cycle_char_color_access(vicii_base_t* vicii, int unused_param_vmli) {
    // Char/color access for cycles 16-54 (cycle 16 wrapper adds MCBASE update before this)
    // VICE reference (vicii-fetch.c vicii_fetch_idle_c): No DEN guard.
    // The decision uses !idle_state (= display_state) || bad_line.
    // VICE uses instantaneous bad_line (not latched). If CPU writes $D011 mid-line
    // to clear the bad line condition, c-access stops for subsequent cycles.
    if (vicii->video_logic.is_bad_line) {
        return VIC_ACCESS_C; // Will FALLTHROUGH in vicii_tick PHI1 phase to VIC_ACCESS_G as well
    } else if (vicii->video_logic.display_state) {
        // On non-bad lines during display state, still need G-access for graphics data
        return VIC_ACCESS_G;
    }
    return VIC_ACCESS_IDLE;
}

// Cycle 16: MCBASE update + first char/color access
static uint8_t vicii_cycle_16_mcbase_char_color(vicii_base_t* vicii, int param_vmli) {
    vicii_sprite_mcbase_update(vicii);
    return vicii_cycle_char_color_access(vicii, param_vmli);
}

static uint8_t vicii_cycle_idle(vicii_base_t* vicii, int unused_param) {
    // Idle cycle: VIC accesses during PHI1, CPU can use PHI2
    // BA/AEC will be set centrally in vicii_tick based on look-ahead
    return VIC_ACCESS_IDLE;
}

// PAL Cycle 0: Sprite 3 P-access + HSync begins
// HSync starts during cycle 0's 8-pixel span for all VIC-II variants.
static uint8_t vicii_cycle_sprite_p_0_pal(vicii_base_t* vicii, int param) {
    vicii->drive_flags_ = vicii->drive_flags_ | SyncFlag::HSync;
    return vicii_cycle_sprite_p_access(vicii, param);
}

// PAL Cycle 1 wrapper: Execute line 0 operations here (delayed from cycle 0)
//
// Documentation (vic-ii.txt lines 1006-1015):
// "Raster line 0 is, however, an exception: In this line, IRQ and incrementing
// (resp. resetting) of RASTER are performed one cycle later than in the other lines.
// But for simplicity we assume equal line lengths and define the beginning of raster
// line 0 to be one cycle before the occurrence of the IRQ."
//
// "Note: After the end of raster line 311 in the 6569, the start of frame (line 0)
// occurs one cycle late. Line timing wraps normally at cycle 63, but the transition
// from line 311 to line 0 introduces this one-cycle delay."
//
// IMPLEMENTATION: This function naturally implements BOTH timing anomalies:
// 1. Line 0 operations delayed by one cycle (executed in cycle 1 instead of cycle 0)
// 2. Frame wrap delay (311→0 transition delayed by one cycle)
//
// The same mechanism handles both cases: By executing line 0 operations in cycle 1
// (instead of cycle 0 as NTSC does), the PAL VIC-II automatically introduces the
// documented one-cycle delay for both the line 0 IRQ/raster operations AND the
// frame boundary transition.
static uint8_t vicii_cycle_sprite_s_1_pal(vicii_base_t* vicii, int param) {
    // IMPORTANT: Cycle functions execute BEFORE vicii_timing_advance(), so raster_counter
    // still contains the PREVIOUS line number. When we're on the last line (311 for PAL),
    // we know the NEXT line will be line 0, so we perform the line 0 operations here.
    const bool transitioning_to_line0 = (vicii->timing.raster_counter == vicii->cached_total_lines - 1);
    
    if (transitioning_to_line0) {
        // Execute line 0 raster/IRQ operations (delayed by one cycle on PAL)
        // This handles both:
        // - Line 0 IRQ timing anomaly (IRQ occurs in cycle 1, not cycle 0)
        // - Frame wrap delay (311→0 transition delayed by one cycle)
        vicii_perform_line0_raster_irq_operations(vicii);
    }
    
    // Call underlying cycle function (sprite 3 S-access for PAL)
    return vicii_cycle_sprite_s_access(vicii, param);
}

// NTSC Cycle 0 wrapper: Execute raster/IRQ operations immediately (no delay)
//
// Documentation: Unlike PAL (6569), NTSC VIC-II chips (6567) do NOT have the
// one-cycle delay for line 0 operations. The frame wrap (last_line→0 transition)
// happens immediately in cycle 0.
//
// IMPLEMENTATION: This function executes line 0 operations in cycle 0, providing
// immediate frame wrap without the one-cycle delay present in PAL chips.
static uint8_t vicii_cycle_sprite_p_0_ntsc(vicii_base_t* vicii, int param) {
    // HSync starts during cycle 0's 8-pixel span for all VIC-II variants.
    vicii->drive_flags_ = vicii->drive_flags_ | SyncFlag::HSync;

    // Check if we're transitioning into line 0
    const bool transitioning_to_line0 = (vicii->timing.raster_counter == vicii->cached_total_lines - 1);
    
    if (transitioning_to_line0) {
        // NTSC: Execute immediately in cycle 0 (no delay, unlike PAL)
        // This handles the frame wrap (last_line→0) without delay
        vicii_perform_line0_raster_irq_operations(vicii);
    }
    
    // Call underlying cycle function (sprite 3 P-access for NTSC)
    return vicii_cycle_sprite_p_access(vicii, param);
}

// Cycle 5: HSync ends + color burst gate opens
// HSync pulse ends and burst gate opens during cycle 5 for all variants.
static uint8_t vicii_cycle_sprite_s_5_hsync_burst(vicii_base_t* vicii, int param) {
    vicii->drive_flags_ = (vicii->drive_flags_ & ~SyncFlag::HSync) | SyncFlag::Burst;
    return vicii_cycle_sprite_s_access(vicii, param);
}

// Cycle 10: Color burst gate closes
// Burst gate closes during cycle 10 for all variants.
static uint8_t vicii_cycle_refresh_burst_off(vicii_base_t* vicii, int param) {
    vicii->drive_flags_ = vicii->drive_flags_ & ~SyncFlag::Burst;
    return vicii_cycle_refresh(vicii, param);
}

// Helper: Sprite Y-coordinate matching (shared by cycles 55 and 56)
// VICE reference (viciisc/vicii-cycle.c check_sprite_dma):
//   For each sprite: if enabled($D015) AND Y matches AND !dma → turn on DMA
//   turn_sprite_dma_on: sprite_dma |= bit, mcbase=0, exp_flop=1
static void vicii_sprite_y_coordinate_check(vicii_base_t* vicii) {
    uint8_t mxe_reg = vicii->regs_[reg::MXE];
    uint16_t raster = vicii->timing.raster_counter;
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
        
        // Cycles 55 and 56: Check Y-coordinate match
        if ((mxe_reg & (1 << i)) && !sprite->dma_enabled) {
            uint8_t sprite_y = vicii->regs_[reg::M0Y + i * 2];
            if ((raster & 0xFF) == sprite_y) {
                // VICE: turn_sprite_dma_on() — DMA on, mcbase=0, exp_flop=1
                // exp_flop is ALWAYS set to 1 (true), regardless of Y-expansion.
                // The expansion toggle at cycle 56 will flip it if needed.
                sprite->dma_enabled = true;
                sprite->mcbase = 0;
                sprite->expansion_flip_flop = true;
            }
        }
    }
}

// Helper: Expansion flip-flop toggle (cycle 56 Phi2)
// VICE reference (viciisc/vicii-cycle.c check_exp):
//   For each sprite: if DMA active AND Y-expanded → toggle exp_flop
static void vicii_sprite_expansion_toggle(vicii_base_t* vicii) {
    uint8_t mxye_reg = vicii->regs_[reg::MXYE];
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
        if (sprite->dma_enabled && (mxye_reg & (1 << i))) {
            sprite->expansion_flip_flop = !sprite->expansion_flip_flop;
        }
    }
}

// Cycle 55: Sprite Y-match + c/g access
static uint8_t vicii_cycle_char_color_y_match(vicii_base_t* vicii, int unused_param_vmli) {
    vicii_sprite_y_coordinate_check(vicii);
    return vicii_cycle_char_color_access(vicii, unused_param_vmli);
}

// Cycle 56: Sprite Y-match + expansion flip-flop toggle + idle access
// VICE timing: ChkSprDma at Phi1(56), ChkSprExp at Phi2(56)
static uint8_t vicii_cycle_idle_y_match(vicii_base_t* vicii, int unused_param) {
    vicii_sprite_y_coordinate_check(vicii);
    vicii_sprite_expansion_toggle(vicii);
    return vicii_cycle_idle(vicii, unused_param);
}

// Cycle 58: RC check, VCBASE update, and display state control (Rule 5 from Section 3.7.2)
static inline void vicii_cycle_58_rc_check(vicii_base_t* vicii) {
    // Spec (lines 1248-1251): "In the first phase of cycle 58, the VIC checks if RC=7.
    // If so, the video logic goes to idle state and VCBASE is loaded from VC (VC->VCBASE).
    // If the video logic is in display state afterwards (this is always the case
    // if there is a Bad Line Condition), RC is incremented."
    //
    // Spec (lines 1196-1203): Cycle 58 controls display state flip-flop:
    // - Rule 1: "If the Bad Line Condition is false in cycle 58, display state is cleared"
    // - Rule 2: "If the Bad Line Condition is true in cycle 58, display state is set"
    
    // First: Handle RC==7 transition - update VCBASE
    bool rc_was_7 = (vicii->video_logic.rc == 7);
    if (rc_was_7) {
        vicii->video_logic.vcbase = vicii->video_logic.vc;
    }
    
    // Second: Update display state based on bad line condition (spec lines 1196-1203)
    // This must happen BEFORE the RC increment check
    if (vicii->video_logic.is_bad_line) {
        vicii->video_logic.display_state = true;
    } else if (rc_was_7) {
        // Only clear display state when RC==7 AND no bad line
        vicii->video_logic.display_state = false;
    }
    
    // Third: Increment RC if in display state (checked AFTER display state rules applied)
    // Spec: "If the video logic is in display state afterwards, RC is incremented"
    if (vicii->video_logic.display_state) {
        vicii->video_logic.rc = (vicii->video_logic.rc + 1) & 0x07;
    }

    // VICE-compatible sprite display state management (Rules 4 and 5)
    // Reference: VICE viciisc/vicii-cycle.c check_sprite_display()
    //
    // MCBASE advancement and DMA termination now happen at cycle 16 (sprite_mcbase_update).
    // Expansion flip-flop toggle happens at cycle 56 (check_exp).
    // Cycle 58 only handles: MC ← MCBASE, display state on/off.
    //
    // Rule 4: "MC is loaded from MCBASE. If DMA is on, display is turned on."
    // Rule 5: "If DMA is off, display is turned off."
    // CRITICAL FIX: Display state depends ONLY on DMA being active, NOT on Y-match.
    // Y-match already happened at cycles 55/56 to enable DMA. On subsequent lines,
    // DMA remains on (until mcbase==63 at cycle 16) but the raster counter no longer
    // matches sprite Y — the old code only displayed the first line of each sprite.
    // VICE reference: viciisc/vicii-cycle.c check_sprite_display() sets display=1
    // unconditionally when DMA is active.
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
        
        // Load MC from MCBASE (always, per VICE check_sprite_display)
        sprite->mc = sprite->mcbase;
        
        if (sprite->dma_enabled) {
            // DMA on → display on, unconditionally
            sprite->display_state = true;
        } else {
            // Rule 5: Turn off display if DMA is off
            sprite->display_state = false;
        }
    }
}

// Cycle 58: RC check and VCBASE update, then sprite S access with MC load
// Combines Rule 5 (Section 3.7.2) and Rule 4 (Section 3.8.1)
static uint8_t vicii_cycle_sprite_p_rc_mc_load(vicii_base_t* vicii, int param_sprite_num) {
    vicii_cycle_58_rc_check(vicii);
    return vicii_cycle_sprite_p_access(vicii, param_sprite_num);
}

static uint8_t vicii_cycle_sprite_s_border_check(vicii_base_t* vicii, int param_sprite_num) {
    // Border Rules 2 & 3: Y coordinate checks in cycle 63 (1-based numbering).
    // Reuses the same two-stage vborder latch logic as raster-advance and $D011 writes.
    vicii_check_vertical_border(vicii);
    // Perform the sprite S access for this cycle
    return vicii_cycle_sprite_s_access(vicii, param_sprite_num);
}
// ========================================================================================
// BUS CONTROL HELPERS - Hardware Connection Details
// ========================================================================================
//
// According to VIC-II documentation (Section 2.4.3, lines 212-230, 406-433):
//
// BA (Bus Available) → 6510 RDY (Ready):
// - VIC-II BA output is connected to 6510 RDY input
// - Normally HIGH: Bus is available to CPU during PHI2
// - Goes LOW 3 cycles BEFORE VIC will need PHI2 access (warning signal)
// - When RDY goes LOW, CPU halts on the NEXT READ cycle (writes can still complete)
// - Returns HIGH when VIC no longer needs PHI2 access
//
// AEC (Address Enable Control) → 6510 AEC (Address Enable Control):
// - VIC-II AEC output is connected to 6510 AEC input
// - Normally LOW during PHI1 (VIC accesses), HIGH during PHI2 (CPU accesses)
// - When BA goes low, AEC continues to follow φ2 for 3 cycles normally
// - After 3 cycles, AEC STAYS LOW during PHI2 (VIC controls address/data lines)
// - When AEC is LOW, the 6510's address bus is tri-stated (disconnected)
// - Returns HIGH when VIC releases the bus
//
// The 3-Cycle Dance (from documentation timing diagram, lines 971-992):
// 1. Cycle N:   BA goes LOW (RDY→LOW, CPU starts halting on reads), AEC still follows φ2
// 2. Cycle N+1: BA is LOW (RDY LOW), AEC still follows φ2, CPU can complete writes
// 3. Cycle N+2: BA is LOW (RDY LOW), AEC still follows φ2, CPU can complete writes
// 4. Cycle N+3: BA is LOW (RDY LOW), AEC now STAYS LOW → VIC has full bus control
//
// Timing sequence (Documentation Section 2.4.3, lines 406-410):
// "BA will then go low 3 cycles before the VIC takes over the bus completely
//  (3 cycles is the maximum number of successive write accesses of the 6510).
//  After 3 cycles, AEC stays low during the second clock phase so that the
//  VIC can output its addresses."
//
// Why 3 cycles? (Documentation lines 217-222):
// "BA is connected to the RDY line of the processor... but this line is ignored
//  on write accesses (the CPU can only be interrupted on reads), and the 6510
//  never does more than three writes in sequence."
//
// BA goes LOW 3 cycles in advance for:
// 1. Bad Line c-accesses (BA low in cycles 12-14, c-accesses in cycles 15-54)
// 2. Sprite p-accesses (sprite data pointer reads)
// 3. Sprite s-accesses (sprite data reads)
//
// BA returns HIGH when VIC no longer needs PHI2 access.

// Direct single-pin helpers for BA/AEC control.
// Uses BUS_SET_BIT / BUS_CLR_BIT (single bit-or / bit-and-not) instead of
// the legacy bus_lines_extract → modify → bus_lines_apply roundtrip, which did
// 12 conditional branches per call through bus_lines_extract/bus_lines_apply.
static inline bus_state_t vicii_bus_control_aec_high(bus_state_t bus_state) {
    BUS_SET_BIT(bus_state, BUS_AEC_BIT);
    return bus_state;
}

static inline bus_state_t vicii_bus_control_aec_low(bus_state_t bus_state) {
    BUS_CLR_BIT(bus_state, BUS_AEC_BIT);
    return bus_state;
}

static inline bus_state_t vicii_bus_control_ba_high(bus_state_t bus_state) {
    // BA HIGH: Bus is available to CPU during PHI2
    // This is the normal/default state
    BUS_SET_BIT(bus_state, BUS_BA_BIT);
    return bus_state;
}

static inline bus_state_t vicii_bus_control_ba_low(bus_state_t bus_state) {
    // BA LOW: VIC will need PHI2 bus access (prevents CPU from accessing bus)
    // This happens during:
    // - Bad Line c-accesses (character pointer reads)
    // - Sprite p-accesses (sprite data pointer reads)
    // - Sprite s-accesses (sprite data reads)
    BUS_CLR_BIT(bus_state, BUS_BA_BIT);
    return bus_state;
}

// ========================================================================================
// CENTRALIZED BUS CONTROL LOGIC
// ========================================================================================

// Check if a specific cycle needs PHI2 bus access (c/p/s access)
// Uses cycle number ranges and cycle table param field for sprite accesses
// IMPORTANT: This function must work correctly for BOTH current cycle AND future cycles (cycle+3)
static inline bool vicii_cycle_needs_phi2_access(vicii_base_t* vicii, uint8_t cycle) {
    if (cycle >= vicii->cached_cycles_per_line) {
        return false;
    }
    
    // Check bad line c-access range (x_cycle 14-54 / spec cycles 15-55)
    // x_cycle 14 (spec 15) is the first c-access cycle (column 0, refresh+c),
    // so BA must go LOW 3 cycles early (at x_cycle 11) to warn the CPU.
    // DEN only affects bad line detection, not sprite accesses
    if (cycle >= 14 && cycle <= 54) {
        // VICE reference: Uses instantaneous bad_line for BA checks.
        // No separate DEN guard needed — is_bad_line already incorporates
        // was_den_set_during_raster_30 (DEN captured at line $30).
        // When CPU writes $D011 mid-line to clear YSCROLL match, BA goes high
        // after the 3-cycle shift register drains (correct hardware behavior).
        return vicii->video_logic.is_bad_line;
    }
    
    // Get sprite number from cycle table param field
    // Sprite P/S accesses are independent of DEN (VICE confirmed)
    const vicii_cycle_entry_t* entry = &vicii->timing.cycle_table[cycle];
    int sprite_num = entry->param; // -1 for non-sprite accesses
    
    if (sprite_num >= 0 && sprite_num < VICII_NUM_SPRITES) {
        // BA must go low for sprites with DMA active (not just $D015 enabled)
        // VICE reference: vicii_check_sprite_ba checks sprite_dma bitmask
        return vicii->sprites.sprites[sprite_num].dma_enabled;
    }
    
    return false;
}

// Centralized function to set BA/AEC based on shift register tracking
// This should be called once per cycle in vicii_tick
static inline bus_state_t vicii_update_ba_aec_signals(vicii_base_t* vicii, bus_state_t bus_state, uint8_t access_type) {
    // Check if we need PHI2 access NOW (current cycle)
    // C-access (including refresh+c at spec cycle 15) always needs PHI2;
    // P/S only need PHI2 when sprite has DMA active
    bool needs_phi2_now = (access_type == VIC_ACCESS_C || access_type == VIC_ACCESS_REFRESH_C);
    if (!needs_phi2_now && (access_type == VIC_ACCESS_P || access_type == VIC_ACCESS_S)) {
        needs_phi2_now = (vicii->bus.active_sprite && vicii->bus.active_sprite->dma_enabled);
    }
    
    if (needs_phi2_now) {
        // Current cycle needs PHI2 access: BA always LOW
        bus_state = vicii_bus_control_ba_low(bus_state);
        
        // AEC 3-cycle delay (documentation section 2.4.3):
        // "AEC always follows Φ2 in the first three cycles after BA has been
        // driven low. If BA is still low after the third cycle, AEC stays low
        // during the Φ2 phase."
        //
        // ba_low_count reflects PREVIOUS cycle's count (updated after this call).
        // For NATURAL bad lines, BA goes LOW 3 cycles early (prediction), so
        // ba_low_count >= 3 by the first c-access → AEC LOW → VIC reads normally.
        // For FORCED bad lines (FLI), BA goes LOW at the first c-access itself,
        // so ba_low_count = 0 → AEC stays HIGH → CPU keeps the bus → VIC's
        // internal data bus drivers are tri-stated (NMOS reads $FF) — the "FLI bug".
        // Color D8-D11 come from CPU D0-D3 via analog switch U16 (active when AEC HIGH).
        if (vicii->bus.ba_low_count >= 3) {
            bus_state = vicii_bus_control_aec_low(bus_state);
        } else {
            bus_state = vicii_bus_control_aec_high(bus_state);
        }
        return bus_state;
    }
    
    // Check shift register: if ANY bit is set, BA should be low
    // Bit 0 = next cycle, bit 1 = cycle+2, bit 2 = cycle+3
    if (vicii->bus.ba_prediction_shift_reg & 0x07) {
        // Future cycle(s) need PHI2 access: BA LOW (warning), AEC HIGH
        bus_state = vicii_bus_control_ba_low(bus_state);
        bus_state = vicii_bus_control_aec_high(bus_state);
    } else {
        // No PHI2 access needed: BA HIGH, AEC HIGH (CPU has bus)
        bus_state = vicii_bus_control_ba_high(bus_state);
        bus_state = vicii_bus_control_aec_high(bus_state);
    }
    
    return bus_state;
}

// ========================================================================================
// PHI1 CYCLE FUNCTION
// ========================================================================================

// PHI1 tick function — processes one VIC-II cycle
bus_state_t vicii_base_t::tick_phi1(bus_state_t bus_state) {
    vicii_base_t* vicii = this;

    // STEP 1: Get current cycle entry and parameter
    const vicii_cycle_entry_t* entry = &vicii->timing.cycle_table[vicii->timing.x_cycle];
    const int access_param = entry->param;

    // Call cycle function to determine current access type, returning either
    // VIC_ACCESS_IDLE, VIC_ACCESS_REFRESH, VIC_ACCESS_P, VIC_ACCESS_S,
    // VIC_ACCESS_C, or VIC_ACCESS_REFRESH_C
    const uint8_t access_type = entry->func(vicii, access_param);

    // CRITICAL: Shift register update BEFORE setting BA/AEC
    //
    // The 3-bit shift register implements the 3-cycle advance warning for BA signal:
    // - Bit 0: next cycle (cycle+1) needs PHI2 access
    // - Bit 1: cycle+2 needs PHI2 access
    // - Bit 2: cycle+3 needs PHI2 access
    //
    // Hardware requirement: BA must go LOW 3 cycles BEFORE VIC needs PHI2 bus access.
    // This gives CPU time to finish up to 3 consecutive write operations.
    //
    // CORRECT OPERATION (shift-right pipeline):
    // Each cycle we:
    // 1. Shift right: predictions move one step closer to "now"
    // 2. Check if cycle+3 needs PHI2 access
    // 3. Set bit 2 if cycle+3 needs access (furthest future slot)
    //
    // Example: Cycle 15 needs c-access (bad line)
    // - Cycle 12: Check cycle 15 → set bit 2 → BA goes LOW (3 cycles early)
    // - Cycle 13: Shift right → bit 2→bit 1 → BA still LOW (2 cycles early)
    // - Cycle 14: Shift right → bit 1→bit 0 → BA still LOW (1 cycle early)
    // - Cycle 15: Shift right → bit 0→(gone), check current access_type → VIC takes bus (AEC LOW)
    
    // Shift right: bit2→bit1, bit1→bit0, bit0→(discarded)
    vicii->bus.ba_prediction_shift_reg >>= 1;
    
    // Check if cycle+3 needs PHI2 access and set bit 2 (furthest future position)
    uint8_t cycle_plus_3 = (vicii->timing.x_cycle + 3) % vicii->cached_cycles_per_line;
    if (vicii_cycle_needs_phi2_access(vicii, cycle_plus_3)) {
        vicii->bus.ba_prediction_shift_reg |= 0x04;  // Set bit 2
    }
    
    // CRITICAL FIX: Update BA/AEC signals at the START of the cycle (PHI1 phase)
    // This must happen BEFORE the CPU's PHI2 tick so the CPU sees the correct BA state.
    // BA goes low when ANY of the next 3 cycles need PHI2 access (shift register != 0)
    bus_state = vicii_update_ba_aec_signals(vicii, bus_state, access_type);

    // Track consecutive cycles BA has been LOW for AEC 3-cycle delay (FLI bug modeling).
    // Documentation (Section 2.4.3): "After 3 cycles [of BA being LOW], AEC stays low
    // during the second clock phase so that the VIC can output its addresses."
    // During the first 3 cycles after BA drops, the VIC cannot read the main data bus
    // during PHI2 because AEC still follows φ2. This causes the c-accesses to read $FF
    // from the tri-stated data bus — the "FLI bug" that makes columns 0-2 show garbage
    // in Flexible Line Interpretation (FLI) mode.
    const bool ba_is_low = !BUS_GET_BIT(bus_state, BUS_BA_BIT);
    if (ba_is_low) {
        if (vicii->bus.ba_low_count < 4) {
            vicii->bus.ba_low_count++;
        }
    } else {
        vicii->bus.ba_low_count = 0;
    }

    // STEP 2: Perform PHI1 memory accesses via direct read
    // G-access happens EVERY cycle, other accesses are special cases
    uint16_t address;
    
    // Now calculate address for PHI1 read based on cycle type
    switch (access_type) {
        case VIC_ACCESS_S: {
            // S-access PHI1: Read sprite data byte 1 (second byte)
            if (vicii->bus.active_sprite && vicii->bus.active_sprite->dma_enabled) {
                address = (uint16_t)vicii->bus.active_sprite->data_pointer * VICII_SPRITE_DATA_BLOCK
                        + vicii->bus.active_sprite->mc + 1;
            } else {
                address = VICII_IDLE_ADDRESS;
            }
            break;
        }
        case VIC_ACCESS_P: {
            // P-access PHI1: Read sprite pointer from video matrix area
            if (vicii->bus.active_sprite) {
                int sprite_idx = (int)(vicii->bus.active_sprite - &vicii->sprites.sprites[0]);
                address = vicii->memory.vm_base | (0x3F8 + sprite_idx);
            } else {
                address = VICII_IDLE_ADDRESS;
            }
            break;
        }
        case VIC_ACCESS_REFRESH_C:
            // Spec cycle 15 (x_cycle 14): PHI1 is a refresh, PHI2 is the first c-access.
            // Fall through to handle the refresh address — c-access is handled in STEP 4/8.
            FALLTHROUGH;
        case VIC_ACCESS_REFRESH:
            // Refresh cycles use special address
            address = vicii->memory.vm_base | 0x3F00 | vicii->video_logic.refresh_counter;
            vicii->video_logic.refresh_counter--;
            break;
        case VIC_ACCESS_C:
            // C-access cycles (spec 16-55) also perform a g-access during PHI1.
            // The c-access (screen RAM + color RAM reads) happens AFTER the
            // g-access and VMLI/VC increment — see STEP 4 below.
            FALLTHROUGH;
        case VIC_ACCESS_G:
            // G-access: Read graphics data (character ROM or bitmap data)
            // Happens on ALL cycles 16-54 during display state (both bad lines and non-bad lines)
            if (vicii->video_logic.display_state) {
                // Use VMLI (Video Matrix Line Index) hardware register for column position
                // VMLI is incremented after each c-access on bad lines (line 1292)
                // and should be used consistently for both bitmap and text mode addressing
                const uint8_t vmli = vicii->video_logic.vmli;
                
                // Bitmap mode (BMM bit set)?
                if (vicii->sequencer.graphics_mode & 2) {
                    // Bitmap mode: CB13 provides bit 13, VC provides bits 3-12, RC provides bits 0-2
                    // Documentation section 3.7.3.3, line 1455: |CB13| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0| RC2| RC1| RC0|
                    // Use VCBASE + VMLI to get the VC value for this column position
                    const uint16_t vc_for_column = (vicii->video_logic.vcbase + vmli) & VICII_VC_MASK;
                    const uint16_t cb13_bit = vicii->memory.cb_base & (1 << 13);

                    address = cb13_bit | (vc_for_column << 3);
                } else {
                    // Text mode: address uses character code from video matrix
                    // Documentation section 3.7.3.1, line 1334: |CB13|CB12|CB11| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 | RC2| RC1| RC0|
                    const uint8_t char_code = vicii->video_data.video_matrix_line[vmli];

                    address = vicii->memory.cb_base | (char_code << 3);
                }
                address |= vicii->video_logic.rc; // Add row counter (3 bits)
                
                // ECM address masking: hold address lines A9 and A10 low
                // Documentation section 3.5.7: "If the ECM bit is set, the address
                // generator will additionally hold the address lines 9 and 10 low."
                // This applies ONLY to g-access (character/bitmap data), NOT to
                // p-access (sprite pointers), s-access (sprite data), c-access
                // (screen RAM), or refresh accesses.
                if (vicii->sequencer.graphics_mode & 4) {
                    address &= ~(0x03 << 9);
                }
                break;
            }
            // Fall through to idle if display_state is false
            FALLTHROUGH;
        default: // VIC_ACCESS_IDLE or G with display_state==false
            // Idle address
            address = VICII_IDLE_ADDRESS;
            break;
    }

    // Apply CIA2 originating vic-ii bank base (set in vicii_memory_bank_change)
    address |= vicii->memory.bank_base;

    // Perform PHI1 memory read via system-provided callback
    bus_state = vicii->bus.mem_read(vicii->bus.mem_read_ctx, bus_state, address);

    // Post-PHI1 read: Handle sprite pointer/data storage immediately
    // P PHI1 reads the sprite pointer, S PHI1 reads data byte 1
    if (access_type == VIC_ACCESS_P && vicii->bus.active_sprite) {
        vicii->bus.active_sprite->data_pointer = BUS_GET_DATA(bus_state);
        vicii->bus.active_sprite->shift_reg = 0;
    } else if (access_type == VIC_ACCESS_S && vicii->bus.active_sprite
               && vicii->bus.active_sprite->dma_enabled) {
        vicii->bus.active_sprite->shift_reg |= (uint32_t)BUS_GET_DATA(bus_state) << 8;
    }

    // STEP 3: Load graphics data into line buffer during g-access cycles 15-54 (0-based)
    // x_cycle 14 (spec 15) does a refresh during PHI1, NOT a g-access.
    // The 40 g/c-access cycles are at x_cycle 15-54 (spec cycles 16-55).
    // Note: spec uses 1-based numbering, x_cycle is 0-based.
    // During display_state, we need graphics data for every raster line (not just bad lines)
    // to show different rows of each character.
    //
    // c_access_screen_addr is set by STEP 4 and consumed by STEP 8 to place the
    // screen RAM address on the bus for c64_memory_tick.
    uint16_t c_access_screen_addr = 0;
    if (vicii->video_logic.display_state && vicii->timing.x_cycle >= 15 && vicii->timing.x_cycle <= 54) {
        // G-access happens EVERY cycle during PHI1 (the address calculation above always runs)
        // The graphics sequencer will use the data when in display_state
        // Use VMLI hardware register for column position (matches hardware behavior)
        const uint8_t graphics_data = BUS_GET_DATA(bus_state);
        const uint8_t vmli = vicii->video_logic.vmli;

        // Store graphics data at the correct position in the line buffer (inlined)
        if (vmli < VICII_CHARS_PER_LINE) {
            vicii->sequencer.graphics_line[vmli] = graphics_data;
        }
        
        // Spec (line 1246): "VC and VMLI are incremented after each g-access in display state."
        // CRITICAL: This happens AFTER g-access on BOTH bad lines and non-bad lines.
        // VC is reloaded from VCBASE at cycle 14/15 each line, so both bad and non-bad
        // lines start with VC=VCBASE and increment through the same 40 columns.
        // VCBASE only advances when RC reaches 7 at cycle 58 (end of 8-row char block).
        //
        // Store vmli value BEFORE increment for pixel sequencer
        vicii->sequencer.current_vmli_for_display = vmli;
        
        // Increment after g-access (not during c-access!)
        // Spec line 1246: "VC and VMLI are incremented after each g-access in display state"
        vicii->video_logic.vmli++;
        if (vicii->video_logic.display_state) {
            vicii->video_logic.vc++;
        }
    }
    
    // STEP 4: C-access preparation — screen RAM address + color RAM read
    //
    // On real hardware, the c-access happens during PHI2. Both screen RAM and color RAM
    // are read at the current VC address. Screen RAM goes through the main bus (delegated
    // to c64_memory_tick via STEP 8). Color RAM has separate data lines (D8-D11) and is
    // read inline here.
    //
    // At x_cycle 14 (spec 15, REFRESH_C): vc=vcbase, vmli=0 — fetches column 0.
    // At x_cycle 15-54 (spec 16-55, VIC_ACCESS_C): vc and vmli are post-increment
    // from STEP 3 — fetches columns 1-40.
    // (Column 40 at x_cycle 54 is a valid bus access but vmli=40 is out of buffer range;
    // the data is discarded by bounds checks in both the color write and phi2 delivery.)
    //
    // Both VIC_ACCESS_C and VIC_ACCESS_REFRESH_C guarantee is_bad_line + DEN enabled,
    // so no additional guards are needed here.
    //
    // Reference: VICE viciisc/vicii-fetch.c vicii_fetch_matrix() reads at post-increment
    // vmli/vc, and vicii_fetch_graphics() reads vbuf[vmli] then increments.
    if (access_type == VIC_ACCESS_C || access_type == VIC_ACCESS_REFRESH_C) {
        const uint16_t vc = vicii->video_logic.vc & VICII_VC_MASK;
        c_access_screen_addr = vicii->memory.vm_base | vc;

        // Color RAM read (separate data lines D8-D11, not on main bus)
        // NOTE: When the FLI bug is active (ba_low_count <= 3), color RAM chip select
        // is NOT active because AEC is HIGH → CPU is bus master. The 4-bit analog
        // switch U16 connects CPU D0-D3 to VIC D8-D11 instead.
        // The color value is overwritten with bus data in vicii_tick_phi2 for FLI bug
        // cycles, so we still read color RAM here as normal (it gets overwritten later
        // for FLI bug columns).
        const uint8_t vmli = vicii->video_logic.vmli;
        if (vmli < VICII_CHARS_PER_LINE) {
            bus_state_t temp = bus_state;
            BUS_SET_ADDR(temp, vc);
            temp = MOS2114::bus_read(vicii->colorram, temp);
            vicii->video_data.video_color_line[vmli] =
                static_cast<vicii_color_t>(BUS_GET_DATA(temp) & 0x0F);
        }
    }
    
    // STEP 5: Perform unified pixel sequencing (8 pixels per cycle)
    // Border flip-flops are now updated per-pixel WITHIN the pixel sequencer.
    // This uses the graphics data that was JUST loaded above AND the border
    // flip-flop state updated above.  Also drives the video output per-dot-clock.
    vicii_pixel_sequencer(vicii);
    
    // STEP 5.5: Light pen pin sampling
    // Read LP pin state via callback (control port 1 pin 6 → VIC-II pin 9).
    // vicii_lightpen_set_pin() handles negative-edge detection and coordinate
    // latching (LPX = x_coordinate/2, LPY = raster_counter, one trigger per frame).
    // Must happen BEFORE STEP 7 (timing advance) so the latched coordinates
    // reflect the current cycle's beam position.
    if (vicii->bus.lp_pin_read) {
        bool lp_pin_high = vicii->bus.lp_pin_read(vicii->bus.lp_pin_context);
        vicii_lightpen_set_pin(vicii, lp_pin_high);
    }

    // STEP 6: Handle VIC-II IRQ signaling to CPU
    // The VIC-II can generate interrupts from 4 sources (raster, sprite collision, etc.)
    // When any enabled interrupt is triggered, bit 7 (VICII_IR_IRQ) of register $D019 is set
    // and the VIC-II must assert the IRQ line to notify the CPU
    //
    // IRQ line behavior (Documentation vic-ii.txt lines 2244-2285):
    // "If at least one latch bit and the belonging bit in the enable register is
    // set, the IRQ line is held low and so the interrupt is triggered in the processor."
    //
    // Implementation follows CIA pattern (mos6526.cpp lines 474-489):
    // - IRQ is active-LOW at BUS_IRQ_BIT (bit 33)
    // - When IRQ flag is set: clear bit 33 (assert IRQ)
    // - When IRQ flag is cleared: set bit 33 (release IRQ)
    //
    // CORRECT BEHAVIOR: VIC-II should ONLY assert (clear bit) when it has an interrupt.
    // The pull-up resistor model (system bus default state) already sets
    // IRQ high at the start of each cycle. If we set it here, we would overwrite any IRQ
    // assertion by CIA or other chips. VIC-II should ONLY assert, never explicitly release.
    if (vicii->regs_[reg::IR] & fld::IR_IRQ) {
        // IRQ flag is set - assert IRQ line (active-low, clear bit)
        BUS_CLR_BIT(bus_state, BUS_IRQ_BIT);
    }
    // Do NOT set IRQ high in else clause - pull-up resistor handles that
    
    // STEP 7: Advance x_coordinate (primary counter) and update derived values
    // Note: vicii_timing_advance() now handles flushing and buffer clearing when wrapping to next line
    vicii_timing_advance(vicii);
    vicii_update_badline_condition(vicii);

    // Store pending access type for PHI2 phase
    // Map VIC_ACCESS_REFRESH_C → VIC_ACCESS_C for PHI2 delivery (vicii_tick_phi2 only
    // needs to know it's a c-access, not which PHI1 access type was paired with it).
    vicii->bus.pending_phi2_access_type =
        (access_type == VIC_ACCESS_REFRESH_C) ? VIC_ACCESS_C : access_type;

    // STEP 8: Set up PHI2 memory access on the bus (C/P/S/REFRESH_C accesses only)
    // These will be serviced by c64_memory_tick and read by vicii_tick_phi2
    switch (access_type) {
        case VIC_ACCESS_P:
            // P PHI2: Set up read for sprite data byte 0 (first data byte)
            // Pointer was already read and stored during PHI1 (STEP 2)
            if (vicii->bus.active_sprite && vicii->bus.active_sprite->dma_enabled) {
                address = (uint16_t)vicii->bus.active_sprite->data_pointer * VICII_SPRITE_DATA_BLOCK
                        + vicii->bus.active_sprite->mc;
            } else {
                // No DMA — no PHI2 data read needed
                return bus_state;
            }
            break;
        case VIC_ACCESS_S:
            // S PHI2: Set up read for sprite data byte 2 (third/final data byte)
            if (vicii->bus.active_sprite && vicii->bus.active_sprite->dma_enabled) {
                address = (uint16_t)vicii->bus.active_sprite->data_pointer * VICII_SPRITE_DATA_BLOCK
                        + vicii->bus.active_sprite->mc + 2;
            } else {
                return bus_state;
            }
            break;
        case VIC_ACCESS_REFRESH_C:
        case VIC_ACCESS_C:
            // C-access PHI2: Set up screen RAM read on the bus
            // c64_memory_tick will service this (AEC is LOW, reads VIC bank mapping).
            // vicii_tick_phi2 reads the result and stores at video_matrix_line[vmli].
            address = c_access_screen_addr;
            break;
        default:
            // PHI1 accesses (G/REFRESH/IDLE) are handled inline, not here
            return bus_state;
    }

    bus_state = vicii_bus_memory_setup(vicii, bus_state, address);
    
    // Return the bus state for threaded cycle chaining
    return bus_state;
}

// ========================================================================================
// PHI2 DELIVERY — process data returned by c64_memory_tick
// ========================================================================================
// Called after c64_memory_tick in the same cycle. The PHI1 phase (vicii_tick_phi1) set up
// the bus address in STEP 8. c64_memory_tick serviced it (AEC LOW → VIC bank mapping).
// This function reads the result and stores it in the appropriate VIC-II register.
//
// Because this runs in the same cycle as PHI1, vmli is still valid from STEP 3/4 —
// no cross-cycle storage is needed for c-access column positions.

void vicii_base_t::tick_phi2(bus_state_t bus_state) {
    vicii_base_t* vicii = this;
    const uint8_t bus_data = BUS_GET_DATA(bus_state);

    switch (vicii->bus.pending_phi2_access_type) {
        case VIC_ACCESS_P:
            // P-access PHI2: sprite data byte 0 (first data byte)
            if (vicii->bus.active_sprite && vicii->bus.active_sprite->dma_enabled) {
                vicii->bus.active_sprite->shift_reg = (uint32_t)bus_data << 16;
            }
            // Keep active_sprite alive — the following S-access cycle needs it
            break;
        case VIC_ACCESS_S:
            // S-access PHI2: sprite data byte 2 (third/final data byte)
            if (vicii->bus.active_sprite && vicii->bus.active_sprite->dma_enabled) {
                vicii->bus.active_sprite->shift_reg |= (uint32_t)bus_data;

                // Advance MC by 3 (all 3 bytes consumed across P+S cycle pair)
                vicii->bus.active_sprite->mc = (vicii->bus.active_sprite->mc + 3) & VICII_SPRITE_MC_MAX;
            }
            vicii->bus.active_sprite = NULL;
            break;
        case VIC_ACCESS_C: {
            // C-access PHI2: screen RAM data — store at current vmli position
            // vmli was set during PHI1 (STEP 3/4) and is still valid here
            const uint8_t vmli = vicii->video_logic.vmli;
            if (vicii->video_logic.display_state && vmli < VICII_CHARS_PER_LINE) {
                // FLI bug modeling (Documentation section 3.14.6):
                // "In the first three cycles after BA went low, the VIC reads $ff as
                // character pointers and as color information the lower 4 bits of the
                // opcode after the access to $d011."
                //
                // When BA has been LOW for fewer than 3 cycles, AEC still follows φ2
                // (HIGH during PHI2), so the VIC's D0-D7 data bus drivers are tri-stated.
                // In NMOS technology, floating data lines read as $FF.
                // D8-D11 (color) get the CPU's data bus D0-D3 via analog switch U16
                // (which is active when AEC is HIGH).
                if (vicii->bus.ba_low_count <= 3) {
                    vicii->video_data.video_matrix_line[vmli] = 0xFF;
                    // Color: lower 4 bits of whatever the CPU had on the bus.
                    // bus_data here reflects the memory system response; on real hardware
                    // it would be the CPU's opcode fetch value. Use bus_data as a
                    // reasonable approximation.
                    vicii->video_data.video_color_line[vmli] =
                        static_cast<vicii_color_t>(bus_data & 0x0F);
                } else {
                    vicii->video_data.video_matrix_line[vmli] = bus_data;
                }
            }
            break;
        }
        default:
            break;
    }

    vicii->bus.pending_phi2_access_type = VIC_ACCESS_IDLE;

    // Update bad line condition at the end of PHI2.
    // This catches any VIC register writes (e.g. YSCROLL from $D011) that the CPU
    // performed during this cycle's memory_tick phase. No VIC register changes can
    // occur between here and the next phi1 (CIA phi1 and CPU phi1 don't write VIC regs),
    // so the cycle function at the start of the next phi1 sees the correct bad line state.
    vicii_update_badline_condition(vicii);
#ifdef CERMU_HAS_GUI
    bus_snapshot_ = bus_state;
#endif
}

// ========================================================================================
// CYCLE TABLE — unified builder (Documentation section 3.6.3)
// ========================================================================================
//
// All VIC-II variants share the same cycle structure with only two differences:
//
//   1. Cycle 1–2 (line-0 raster/IRQ handling):
//      PAL (6569): postpones line-0 ops to cycle 2 (one-cycle delay)
//      NTSC (6567): handles line-0 ops immediately in cycle 1
//
//   2. Idle padding between display end and sprite-0 tail:
//      PAL  63 cycles → 1 idle
//      R56A 64 cycles → 2 idles
//      R8   65 cycles → 3 idles
//
// The remaining 57 entries are identical across all variants.
//
static void build_vicii_cycle_table(vicii_cycle_entry_t* table, bool is_pal, int cycles_per_line) {
    int i = 0;

    // ── Cycles 0–1: line-0 raster/IRQ handling (PAL vs NTSC) ────────────
    // Cycle 0 also starts the HSync pulse (all variants).
    if (is_pal) {
        table[i++] = {vicii_cycle_sprite_p_0_pal, 3};    // PAL: sprite 3 P-access + HSync on, line-0 ops postponed
        table[i++] = {vicii_cycle_sprite_s_1_pal, 3};    // PAL: sprite 3 S-access + postponed line-0 ops
    } else {
        table[i++] = {vicii_cycle_sprite_p_0_ntsc, 3};   // NTSC: sprite 3 P-access + HSync on + immediate line-0 ops
        table[i++] = {vicii_cycle_sprite_s_access, 3};    // NTSC: sprite 3 S-access (normal)
    }

    // ── Cycles 2–4: sprite 4 P/S, sprite 5 P ─────────────────────────────
    table[i++] = {vicii_cycle_sprite_p_access, 4};
    table[i++] = {vicii_cycle_sprite_s_access, 4};
    table[i++] = {vicii_cycle_sprite_p_access, 5};

    // ── Cycle 5: sprite 5 S-access + HSync off + Burst on ────────────────
    table[i++] = {vicii_cycle_sprite_s_5_hsync_burst, 5};

    // ── Cycles 6–9: sprite 6–7 P/S accesses ──────────────────────────────
    for (int s = 6; s <= 7; s++) {
        table[i++] = {vicii_cycle_sprite_p_access, s};
        table[i++] = {vicii_cycle_sprite_s_access, s};
    }

    // ── Cycle 10: first DRAM refresh + Burst off ─────────────────────────
    table[i++] = {vicii_cycle_refresh_burst_off, -1};

    // ── Cycles 11–14: remaining refreshes + VC update + sprite crunch ────
    table[i++] = {vicii_cycle_refresh, -1};
    table[i++] = {vicii_cycle_refresh, -1};
    table[i++] = {vicii_cycle_refresh_vc_update, -1};
    table[i++] = {vicii_cycle_refresh_first_c_access_sprite_crunch, -1};

    // ── Cycles 16–54: 40-column character/color access ───────────────────
    table[i++] = {vicii_cycle_16_mcbase_char_color, 0};   // + MCBASE update
    for (int c = 1; c <= 38; c++)
        table[i++] = {vicii_cycle_char_color_access, c};

    // ── Cycle 55–56: display end + sprite Y match ────────────────────────
    table[i++] = {vicii_cycle_char_color_y_match, 39};
    table[i++] = {vicii_cycle_idle_y_match, -1};

    // ── Idle padding: 1 (PAL/63), 2 (R56A/64), 3 (R8/65) ────────────────
    const int idle_count = cycles_per_line - 62;
    for (int j = 0; j < idle_count; j++)
        table[i++] = {vicii_cycle_idle, -1};

    // ── Sprite 0–2 tail (always last 6 cycles) ──────────────────────────
    table[i++] = {vicii_cycle_sprite_p_rc_mc_load, 0};    // Sprite 0 P-access + RC/VCBASE + MC load
    table[i++] = {vicii_cycle_sprite_s_access, 0};
    table[i++] = {vicii_cycle_sprite_p_access, 1};
    table[i++] = {vicii_cycle_sprite_s_access, 1};
    table[i++] = {vicii_cycle_sprite_p_access, 2};
    table[i++] = {vicii_cycle_sprite_s_border_check, 2};
}

// Static storage for the three VIC-II cycle tables (populated on first use)
static vicii_cycle_entry_t vicii_cycle_table_6569[63];
static vicii_cycle_entry_t vicii_cycle_table_6567R56A[64];
static vicii_cycle_entry_t vicii_cycle_table_6567R8[65];
static bool vicii_cycle_tables_built = false;

static void vicii_ensure_cycle_tables() {
    if (vicii_cycle_tables_built) return;
    build_vicii_cycle_table(vicii_cycle_table_6569,     true,  63);  // MOS 6569 PAL
    build_vicii_cycle_table(vicii_cycle_table_6567R56A, false, 64);  // MOS 6567 R56A NTSC
    build_vicii_cycle_table(vicii_cycle_table_6567R8,   false, 65);  // MOS 6567 R8 NTSC
    vicii_cycle_tables_built = true;
}

// ========================================================================================
// INITIALIZATION
// ========================================================================================

static inline void vicii_initialize(vicii_base_t* vicii) {
    // Zero all units
    vicii->regs_.clear();
    memset(&vicii->timing, 0, sizeof(vicii_timing_unit_t));
    memset(&vicii->video_logic, 0, sizeof(vicii_video_logic_unit_t));
    memset(&vicii->video_data, 0, sizeof(vicii_video_data_unit_t));
    memset(&vicii->sequencer, 0, sizeof(vicii_sequencer_unit_t));
    memset(&vicii->border, 0, sizeof(vicii_border_unit_t));
    memset(&vicii->memory, 0, sizeof(vicii_memory_unit_t));
    memset(&vicii->sprites, 0, sizeof(vicii_sprites_unit_t));
    vicii->pixel = vicii_pixel_unit_t{};
    memset(&vicii->bus, 0, sizeof(vicii_bus_unit_t));
    
    // Set default register values - Enable DEN to match real hardware behavior
    // The VIC-II starts with display enabled, allowing immediate character data display
    vicii->regs_[reg::C1] = fld::C1_RST8 | fld::C1_DEN | fld::C1_RSEL |
                            (fld::C1_YSCROLL & 3); // DEN=1, YSCROLL=3, RSEL=1, RST8=1
    vicii->regs_[reg::MXE] = 0;  // All sprites disabled
    vicii->regs_[reg::C2] = fld::C2_CSEL; // 8: XSCROLL:0, no MultiColorMode, 40-column display, no RESET
    vicii->regs_[reg::MP] = fld::MP_CB12 | fld::MP_VM10; // 0x14: "address of Character Dot-Data area to 4096 ($1000)"
    vicii->regs_[reg::RASTER] = 0; // Raster compare bits 0-7
    vicii->regs_[reg::IR] = 0; // No interrupts latched at startup
    // CRITICAL FIX: Disable VIC-II interrupts at startup to prevent boot disruption
    // The KERNAL will enable raster interrupts after initialization is complete
    // Starting with interrupts enabled causes repeated CINT calls that corrupt zero-page
    vicii->regs_[reg::IE] = 0; // No interrupts enabled at startup

    // Initialize lightpen: LP pin starts HIGH (released), not triggered
    vicii->lightpen.lp_pin_prev = true;
    vicii->lightpen.triggered = false;

    // Set default colors
    vicii->regs_[reg::EC] = VICII_COLOR_LIGHT_BLUE; // 14: Border Color
    vicii->regs_[reg::B0C] = VICII_COLOR_BLUE; // 6: Background Color 0
    vicii->regs_[reg::B1C] = VICII_COLOR_WHITE; // 1: Background Color 1
    vicii->regs_[reg::B2C] = VICII_COLOR_RED; // 2: Background Color 2
    vicii->regs_[reg::B3C] = VICII_COLOR_CYAN; // 3: Background Color 3
    vicii->regs_[reg::MM0] = VICII_COLOR_PURPLE; // 4: Sprite Multicolor 0
    vicii->regs_[reg::MM1] = VICII_COLOR_BLACK; // 0: Sprite Multicolor 1
    vicii->regs_[reg::M0C] = VICII_COLOR_WHITE; // 1: Sprite Color 0
    vicii->regs_[reg::M1C] = VICII_COLOR_RED; // 2: Sprite Color 1
    vicii->regs_[reg::M2C] = VICII_COLOR_CYAN; // 3: Sprite Color 2
    vicii->regs_[reg::M3C] = VICII_COLOR_PURPLE; // 4: Sprite Color 3
    vicii->regs_[reg::M4C] = VICII_COLOR_GREEN; // 5: Sprite Color 4
    vicii->regs_[reg::M5C] = VICII_COLOR_BLUE; // 6: Sprite Color 5
    vicii->regs_[reg::M6C] = VICII_COLOR_YELLOW; // 7: Sprite Color 6
    vicii->regs_[reg::M7C] = VICII_COLOR_MEDIUM_GREY; // 12: Sprite Color 7
    
    // Initialize sprite display priorities from default $D01B = 0 (all sprites in front)
    // CRITICAL: memset zeroed sprite structs → priority=0 (BACKGROUND), which prevents
    // sprites from ever winning the display priority check.  $D01B=0 means all sprites
    // display in FRONT of graphics, so priority must be SPRITE_IN_FRONT (3).
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii->sprites.sprites[i].priority = VICII_PRIORITY_SPRITE_IN_FRONT;
        vicii->sprites.sprites[i].expansion_flip_flop = true;  // starts set (not expanded)
    }
    
    // Initialize sequencer
    vicii->sequencer.last_mode = 0xFF;
    vicii->sequencer.shift_reg = 0;
    vicii->sequencer.xscroll_counter = 0;
    
    // Update units based on register values
    vicii_sequencer_update_mode(&vicii->sequencer, vicii->regs_[reg::C1], vicii->regs_[reg::C2]);
    vicii_border_update_limits(&vicii->border, vicii->regs_[reg::C1], vicii->regs_[reg::C2]);
    
    // Initialize border pixel from register value (EC was set to LIGHT_BLUE at line 1815)
    vicii->border.border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border.border_pixel.color = static_cast<vicii_color_t>(uint8_t(vicii->regs_[reg::EC]));
    // Initialize border flip-flops (Documentation section 3.9)
    vicii->border.main_border_flip_flop = true;      // Start with border on
    vicii->border.vertical_border_flip_flop = true;  // Start with vertical border on
    vicii->border.set_vertical_border_flip_flop = true; // Staged latch also starts on
    vicii->border.deferred_right_border = false;      // No pending right border
    vicii_memory_update_mapping(&vicii->memory, vicii->regs_[reg::MP]);
    
    // Initialize refresh counter (Documentation section 3.13)
    vicii->video_logic.refresh_counter = 0xFF;
    
    // Initialize sprite expansion flip-flops
    // Documentation (section 3.8.1, Rule 1): "The expansion flip flop is set as long
    // as the bit in MxYE in register $d017 corresponding to the sprite is cleared."
    // Since MxYE starts at 0 (all bits cleared), all flip-flops should be SET (true).
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii->sprites.sprites[i].expansion_flip_flop = true;
    }
    
    // Initialize bad line detection
    // Since DEN is enabled at startup (line 1716), we must set was_den_set_during_raster_30
    // to true so that bad lines can occur immediately. Without this, the first frame would
    // have no bad lines and therefore no character data display.
    vicii->video_logic.was_den_set_during_raster_30 = true;
    vicii->video_logic.ba_low_for_bad_line = false;
    
    // Pixel buffers will be allocated in timing initialization
}

static inline void vicii_initialize_timing(vicii_base_t* vicii, const VicIITraits& traits) {
    // Ensure cycle tables are built (lazy one-time init)
    vicii_ensure_cycle_tables();

    // Select cycle table based on timing characteristics
    if (traits.cycles_per_line == 63) { // PAL
        vicii->timing.cycle_table = vicii_cycle_table_6569;
    } else if (traits.cycles_per_line == 64) { // NTSC R56A
        vicii->timing.cycle_table = vicii_cycle_table_6567R56A;
    } else if (traits.cycles_per_line == 65) { // NTSC R8
        vicii->timing.cycle_table = vicii_cycle_table_6567R8;
    } else {
        // Default to PAL if unknown
        vicii->timing.cycle_table = vicii_cycle_table_6569;
    }
    
    // Populate all cached values from traits
    vicii->cached_total_lines = traits.total_lines;
    vicii->cached_last_vblank_line = traits.last_vblank_line;
    vicii->cached_first_vblank_line = traits.first_vblank_line;
    vicii->cached_cycles_per_line = traits.cycles_per_line;
    vicii->cached_chip_name = traits.chip_name;

    const uint16_t visible_pixels = traits.visible_pixels_per_line;

    // Allocate pixel buffers based on traits
    if (visible_pixels > 0) {
        // Free any existing buffers
        free(vicii->pixel.pixel_line_priority);
        free(vicii->pixel.color_line);
        free(vicii->pixel.sprite_collision_line);
        free(vicii->pixel.graphics_fg_line);
        
        // Allocate single line buffers
        vicii->pixel.pixel_line_priority = static_cast<vicii_priority_t*>(malloc(visible_pixels * sizeof(vicii_priority_t)));
        vicii->pixel.color_line = static_cast<uint8_t*>(malloc(visible_pixels * sizeof(uint8_t)));
        // Collision detection buffers (independent of display)
        vicii->pixel.sprite_collision_line = static_cast<uint8_t*>(calloc(visible_pixels, sizeof(uint8_t)));
        vicii->pixel.graphics_fg_line = static_cast<bool*>(calloc(visible_pixels, sizeof(bool)));
        
        // Initialize buffer with current border color
        const uint8_t border_color = vicii->regs_[reg::EC] & 0x0F;
        memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, visible_pixels);
        memset(vicii->pixel.color_line, border_color, visible_pixels);
    }
    
    vicii_set_x_cycle(vicii, 0);
    vicii->timing.raster_counter = 0;
    
    // Initialize prev_raster_compare to current value to prevent spurious edge detection
    vicii->timing.prev_raster_compare = vicii_get_raster_compare(vicii);
    
    // Initialize border flip-flops to show border initially
    vicii->border.main_border_flip_flop = true;
    vicii->border.vertical_border_flip_flop = true;
    vicii->border.set_vertical_border_flip_flop = true;
    vicii->border.deferred_right_border = false;

    // Pre-compute display mapping constants for the pixel pipeline.
    // These are session-constant (PAL/NTSC chosen at init) and eliminate
    // per-pixel modulo operations and repeated pointer dereferences.
    const uint16_t ppl = traits.cycles_per_line * 8;
    vicii->cached_pixels_per_line = ppl;
    vicii->cached_visible_pixels = visible_pixels;
    vicii->cached_first_x_coord = traits.first_x_coord;
    vicii->cached_display_offset = ppl + VICII_PIPELINE_DELAY_PIXELS + VICII_X_CENTERING_PIXELS;
    vicii->cached_first_visible_display = (traits.first_visible_x_coord + vicii->cached_display_offset) % ppl;
    vicii->cached_wrap_threshold = (vicii->cached_first_visible_display + vicii->cached_visible_pixels) % ppl;

    // Horizontal analog signal timing
    vicii->cached_hsync_start = traits.hsync_start;
    vicii->cached_hsync_end = traits.hsync_end;
    vicii->cached_burst_start = traits.burst_start;
    vicii->cached_burst_end = traits.burst_end;
    vicii->cached_last_visible_x = traits.last_visible_x_coord;
    vicii->cached_first_visible_x = traits.first_visible_x_coord;

    // Compute signal frame start raster for vertical centering.
    // Goal: equal top and bottom borders around the 200-line text area.
    // Text area: rasters 51-250 (RSEL=1).  VBlank: first_vblank..last_vblank.
    // First visible after VBlank: last_vblank + 1.
    // Top border without centering = (border_top - (last_vblank+1)) lines.
    // Total border = visible_lines - 200.  Target top = total_border / 2.
    // Extra lines needed before first visible = target_top - current_top.
    // Those come from pre-VBlank rasters → frame_start = first_vblank - extra.
    {
        const uint16_t first_vis_raster = traits.last_vblank_line + 1;
        const uint16_t top_border_now = VICII_BORDER_TOP_RSEL1 - first_vis_raster;
        const uint16_t total_border = traits.visible_lines - 200;
        const uint16_t target_top = total_border / 2;
        if (target_top > top_border_now) {
            uint16_t extra = target_top - top_border_now;
            vicii->signal_frame_start_raster_ =
                (traits.first_vblank_line + traits.total_lines - extra) % traits.total_lines;
        } else {
            vicii->signal_frame_start_raster_ = 0;
        }
    }

    // Initialize raster_flags_ and drive_flags_ for the starting position.
    // HSync/Burst will be set by the cycle 0 callback on first tick.
    vicii_update_vblank_flags(vicii);
}

// ========================================================================================
// PUBLIC API FUNCTIONS
// ========================================================================================

void vicii_base_t::init_base(const VicIITraits& traits, void (*bank_change)(void*, uint8_t)) {
    this->traits_ = &traits;
    this->bus.bank_change = bank_change;
    info_ = ChipInfo{traits.chip_id, traits.vendor, traits.chip_name};
    vicii_initialize(this);
    vicii_initialize_timing(this, traits);
    set_named_palettes(c64_named_palettes, c64_named_palette_count);
#ifdef CERMU_HAS_CHIP_DEBUG
    register_debug_fields();
#endif
}

// Destructor — clean up dynamically allocated pixel line buffers
vicii_base_t::~vicii_base_t() {
    free(pixel.pixel_line_priority);
    free(pixel.color_line);
    free(pixel.sprite_collision_line);
    free(pixel.graphics_fg_line);
}

void vicii_base_t::reset() {
    // Preserve externally-owned pointers and configuration that survive reset.
    // vicii_initialize() memsets every unit to zero, so anything the system
    // wired up (callbacks, color RAM) must be saved/restored.
    const VicIITraits* saved_traits = traits_;
    const vicii_bus_unit_t saved_bus = bus;       // entire bus unit (mem_read, bank_change, etc.)
    MOS2114* saved_colorram = colorram;

    // Re-initialize all state (zeroes + defaults)
    // vicii_initialize_timing will free/re-allocate pixel line buffers
    vicii_initialize(this);
    vicii_initialize_timing(this, *saved_traits);

    // Restore preserved pointers and callbacks
    traits_ = saved_traits;
    bus = saved_bus;
    // Zero runtime state within bus that should be cleared on reset
    bus.pending_phi2_access_type = 0;
    bus.active_sprite = nullptr;
    bus.ba_prediction_shift_reg = 0;
    bus.ba_low_count = 0;
    bus.bus_line_mask = 0;
    colorram = saved_colorram;
}

// set_framebuffer removed — system manages display via VideoPort output.

// ============================================================================
// Debug field registration (populates ChipDebugRegistry for the default
// two-column debug layout provided by ChipBase)
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG

void vicii_base_t::register_debug_fields() {
    using VI = const vicii_base_t;
    auto& r = debug_registry_;
    wire_debug_registers(VICII_REG_INFO, 0xD000);
    r.set_decl_entries(VICII_DECL_ENTRIES.data(), VICII_DECL_ENTRIES.size());
    r.set_palette(get_default_palette(), 16);

    // Screen mode names indexed by (ECM<<2 | BMM<<1 | MCM)
    static constexpr const char* screen_mode_names[] = {
        "Standard Text", "Multicolor Text", "Standard Bitmap",
        "Multicolor Bitmap", "Extended Color Text", "Invalid Mode",
        "Invalid Mode", "Invalid Mode"
    };

    // ---- Chip Information ----
    r.category("Chip Information")
     .value("Cycles/Line", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->cached_cycles_per_line; }, 8)
     .value("Total Lines", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->cached_total_lines; }, 16)
     .value("Current Bank", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->memory.bank_base / 0x4000; }, 8);

    // ---- Raster Information ----
    r.category("Raster Information")
     .raster_position("Position",
         +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timing.raster_counter; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timing.x_cycle; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->cached_total_lines; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->cached_cycles_per_line; })
     .flag("Badline", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->video_logic.is_bad_line; })
     .value("X Coordinate", +[](const ChipBase* c) -> uint32_t { return static_cast<VI*>(c)->timing.x_coordinate; }, 16);

    // ---- Display Mode (derived from multiple control registers) ----
    r.category("Display Mode")
     .state("Screen Mode", +[](const ChipBase* c) -> uint32_t {
         auto* s = static_cast<VI*>(c);
         uint8_t cr1 = s->regs_[reg::C1];
         uint8_t cr2 = s->regs_[reg::C2];
         uint8_t ecm = (cr1 >> 6) & 1;
         uint8_t bmm = (cr1 >> 5) & 1;
         uint8_t mcm = (cr2 >> 4) & 1;
         return (ecm << 2) | (bmm << 1) | mcm;
     }, screen_mode_names, 8);
}
#endif // CERMU_HAS_CHIP_DEBUG
