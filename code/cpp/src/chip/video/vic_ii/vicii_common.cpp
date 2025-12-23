#include "vicii_common.h"
#include "../../memory/mos2114.h"
#include "../../../systems/c64/c64_bus.h"
#include "../../../core/system_lines.h"
#include <cstdint>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ========================================================================================
// CONSTANTS AND STATIC DATA
// ========================================================================================

// VIC-II pipeline delay: Data fetched at position X is displayed 12 pixels later
// Documentation (vic-ii.txt line 875-876): "the read graphics data is not
// immediately displayed on the screen (there is a delay of 12 pixels)"
// This means we need to SUBTRACT the delay when writing to the line buffer
constexpr int16_t VICII_PIPELINE_DELAY_PIXELS = 12;

// C64 color palette - RGBA format
static const uint32_t c64_palette[16] = {
    0xFF000000, 0xFFFFFFFF, 0xFF2B3768, 0xFFB2A470,
    0xFF863D6F, 0xFF438D58, 0xFF792835, 0xFF6FC7B8,
    0xFF254F6F, 0xFF003943, 0xFF59679A, 0xFF444444,
    0xFF6C6C6C, 0xFF84D29A, 0xFFB55E6C, 0xFF959595
};

// ========================================================================================
// INLINE UTILITY FUNCTIONS
// ========================================================================================

// Get default palette
const uint32_t* vicii_get_default_palette(void) {
    return (uint32_t*)c64_palette;
}

// ========================================================================================
// BORDER LOGIC
// ========================================================================================

static inline void vicii_border_update_limits(vicii_border_unit_t* border, uint8_t c1_reg, uint8_t c2_reg) {
    border->border_top = (c1_reg & VICII_C1_RSEL) ?
        VICII_BORDER_TOP_RSEL1 : VICII_BORDER_TOP_RSEL0;
    border->border_bottom = (c1_reg & VICII_C1_RSEL) ?
        VICII_BORDER_BOTTOM_RSEL1 : VICII_BORDER_BOTTOM_RSEL0;
    border->border_left = (c2_reg & VICII_C2_CSEL) ?
        VICII_BORDER_LEFT_CSEL1 : VICII_BORDER_LEFT_CSEL0;
    border->border_right = (c2_reg & VICII_C2_CSEL) ?
        VICII_BORDER_RIGHT_CSEL1 : VICII_BORDER_RIGHT_CSEL0;
}

// ========================================================================================
// PIXEL SEQUENCER AND GRAPHICS - HARDWARE-ACCURATE ARCHITECTURE
// ========================================================================================

// CRITICAL INSIGHT: Real VIC-II hardware separation of concerns:
// 1. G-access cycles load graphics data into 8-bit shift register
// 2. Pixel sequencer reads from shift register and outputs exactly 8 pixels per cycle
// 3. Each cycle produces exactly 8 pixels, regardless of graphics mode
// 4. Multicolor modes consume 2 bits per pixel, standard modes consume 1 bit per pixel

// X-coordinate driven pixel emission for precise positioning
// The line buffer contains pixels in display order (0-402 for PAL visible area).
// x_coordinate values wrap around (0-503 for PAL), so we must map them correctly.
static inline void vicii_pixel_emit_at_x(vicii_t* vicii, const vicii_pixel_t* pixel_data, uint16_t x_coord) {
    // The x_coord parameter is the DISPLAY position where this pixel should appear.
    // The 12-pixel pipeline delay is already accounted for in the caller (pixel sequencer).
    // We just need to map this x_coordinate to the correct line buffer position.
    const uint16_t pixels_per_line = vicii->config->cycles_per_line * 8;
    const uint16_t display_x_coord = x_coord;
    
    // Check if this coordinate is in the visible range
    // For PAL: first_visible_x_coord = 480, visible_pixels = 403
    // Visible range wraps: 480-503 (24 pixels), then 0-378 (379 pixels) = 403 total
    const uint16_t first_visible = vicii->config->first_visible_x_coord;
    const uint16_t visible_pixels = vicii->config->visible_pixels_per_line;
    
    // Calculate position in line buffer (0-402)
    uint16_t buffer_pos;
    if (display_x_coord >= first_visible) {
        // First part of visible range (480-503 for PAL)
        buffer_pos = display_x_coord - first_visible;
    } else if (display_x_coord < (first_visible + visible_pixels) % pixels_per_line) {
        // Second part of visible range after wrap (0-378 for PAL)
        buffer_pos = (pixels_per_line - first_visible) + display_x_coord;
    } else {
        // Not in visible range
        return;
    }
    
    if (buffer_pos < visible_pixels) {
        vicii->pixel.pixel_line_priority[buffer_pos] = pixel_data->priority;
        vicii->pixel.pixel_line_color[buffer_pos] = pixel_data->color;
    }
}

// Graphics sequencer (called from G-access) - Stores graphics data in line buffer ONLY
// The vmli parameter comes from the cycle table's param field (0-39 for cycles 15-54)
// and is used to store graphics data at the correct position in the line buffer.
// According to vic-ii.txt documentation, the hardware uses VMLI (Video Matrix Line Index)
// to track position within the internal 40×12 bit video matrix/color line buffer.
void vicii_graphics_sequencer(vicii_t* vicii, uint8_t graphics_data, uint8_t vmli) {
    vicii_sequencer_unit_t* seq = &vicii->sequencer;
    
    // Store graphics data in the line buffer at the specified character index
    // The pixel sequencer will load from this buffer using VMLI when rendering
    if (vmli < 40) {
        seq->graphics_line[vmli] = graphics_data;
    }
}

// ========================================================================================
// SPRITE HANDLING
// ========================================================================================

static inline void vicii_sprite_emit_pixels(vicii_t* vicii, int param_sprite_num) {
    vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[param_sprite_num];
    
    if (!sprite->enabled || !sprite->display_state) return;
    
    const uint16_t sprite_x =
         (vicii->registers.data[VICII_M0X + param_sprite_num * 2]) |
        ((vicii->registers.data[VICII_MX8] & (1 << param_sprite_num)) ? 0x100 : 0);
    
    // Use x_coordinate for sprite positioning (matches sprite coordinate system)
    // Sprites are positioned relative to x_coordinate, not x_cycle
    const uint16_t current_x = vicii->timing.x_coordinate;
    
    if (current_x < sprite_x || current_x >= (sprite_x + 24)) return;
    
    uint8_t sprite_pixel_x = (uint8_t)(current_x - sprite_x);
    
    if (vicii->registers.data[VICII_MXXE] & (1 << param_sprite_num)) {
        sprite_pixel_x >>= 1;
    }
    
    const uint32_t pixel_mask = 0x800000 >> sprite_pixel_x;
    const bool sprite_pixel = (sprite->shift_reg & pixel_mask) != 0;
    
    if (!sprite_pixel) return;
    
    // Apply 12-pixel pipeline delay: sprite rendered at current_x is displayed 12 pixels later
    // Use the same vicii_pixel_emit_at_x coordinate mapping as background graphics
    const uint16_t display_x_coord = current_x + VICII_PIPELINE_DELAY_PIXELS;
    
    // Calculate pixel position in line buffer using same mapping as background
    const uint16_t pixels_per_line = vicii->config->cycles_per_line * 8;
    const uint16_t first_visible = vicii->config->first_visible_x_coord;
    const uint16_t visible_pixels = vicii->config->visible_pixels_per_line;
    
    uint16_t pixel_line_x;
    if (display_x_coord >= first_visible) {
        pixel_line_x = display_x_coord - first_visible;
    } else if (display_x_coord < (first_visible + visible_pixels) % pixels_per_line) {
        pixel_line_x = (pixels_per_line - first_visible) + display_x_coord;
    } else {
        return;  // Not in visible range
    }
    
    if (pixel_line_x >= visible_pixels) return;
    
    vicii_priority_t current_priority = vicii->pixel.pixel_line_priority[pixel_line_x];
    
    // Collision detection
    if (current_priority == VICII_PRIORITY_SPRITE_IN_FRONT ||
        current_priority == VICII_PRIORITY_SPRITE_BEHIND) {
        vicii->registers.data[VICII_MXM_2] |= (1 << param_sprite_num);
    }
    
    if (current_priority == VICII_PRIORITY_FOREGROUND) {
        vicii->registers.data[VICII_MXD_2] |= (1 << param_sprite_num);
    }
    
    // Color determination
    uint8_t sprite_color;
    const bool is_multicolor = (vicii->registers.data[VICII_MXMC] & (1 << param_sprite_num)) != 0;
    
    if (is_multicolor) {
        const uint8_t bit_pair = (sprite->shift_reg >> (22 - sprite_pixel_x)) & 3;
        switch (bit_pair) {
            default: //  avoids a compiler warning
            case 0: return;
            case 1: sprite_color = vicii->registers.data[VICII_MM0]; break;
            case 2: sprite_color = vicii->registers.data[VICII_M0C + param_sprite_num]; break;
            case 3: sprite_color = vicii->registers.data[VICII_MM1]; break;
        }
    } else {
        sprite_color = vicii->registers.data[VICII_M0C + param_sprite_num];
    }
    
    // Priority check
    bool sprite_wins = false;
    if (sprite->priority == VICII_PRIORITY_SPRITE_IN_FRONT) {
        sprite_wins = true;
    } else if (sprite->priority == VICII_PRIORITY_SPRITE_BEHIND) {
        sprite_wins = (current_priority <= VICII_PRIORITY_BACKGROUND);
    }

    if (sprite_wins) {
        vicii->pixel.pixel_line_priority[pixel_line_x] = sprite->priority;
        vicii->pixel.pixel_line_color[pixel_line_x] = sprite_color;
    }
}

// Sprite sequencer 
void vicii_sprite_sequencer(vicii_t* vicii) {
    for (int i = VICII_NUM_SPRITES - 1; i >= 0; i--) {
        vicii_sprite_emit_pixels(vicii, i);
    }
}

// Pixel sequencer - sequences exactly 8 pixels per cycle
// This is the ONLY function that emits pixels to the framebuffer
static void vicii_pixel_sequencer(vicii_t* vicii) {
    // Use x_coordinate directly from the timing unit.
    // The VIC-II fetches graphics data at x_coordinate, but those pixels are displayed
    // 12 pixels later due to the pipeline delay. The vicii_pixel_emit_at_x() function
    // handles this delay by adding 12 pixels when writing to the line buffer.
    //
    // This ensures CPU register writes (like border/background color changes) affect
    // pixels being OUTPUT at that moment, not pixels being LOADED into the pipeline.
    const uint16_t x_coord = vicii->timing.x_coordinate;
    
    // Determine if we're in border or display area
    // VIC-II border flip-flop logic: graphics are displayed when main_border_flip_flop is FALSE
    // Border is displayed when main_border_flip_flop is TRUE
    const bool in_main_display = !vicii->border.main_border_flip_flop
                              && !vicii->border.vertical_border_flip_flop;
    
    if (!(in_main_display && vicii->video_logic.display_state)) {
            // We're in border area - sequence exactly 8 border pixels
            for (int pixel = 0; pixel < 8; pixel++) {
                // Apply 12-pixel pipeline delay: border rendered at x_coord is displayed 12 pixels later
                const uint16_t pixel_x = x_coord + (uint16_t)pixel + VICII_PIPELINE_DELAY_PIXELS;
    
                vicii_pixel_emit_at_x(vicii, &vicii->border.border_pixel, pixel_x);
            }
    } else {
        // We're in display area - sequence 8 pixels from shift register
        vicii_sequencer_unit_t* seq = &vicii->sequencer;
        
        // Column index is managed by the g-access cycle functions and stored in graphics_line buffer
        // The pixel sequencer reads from the buffer position corresponding to the current column
        // The 12-pixel pipeline delay (applied in vicii_pixel_emit_at_x) handles the timing
        // between data fetch and screen display
        
        // Initialize xscroll on first display cycle of each line
        // This must happen at the FETCH position (x_coord), not the output position
        // because we're setting up state for the pixel sequencer to use
        const uint16_t pixels_per_line = vicii->config->cycles_per_line * 8;
        const uint16_t first_visible = vicii->config->first_visible_x_coord;
        
        // Calculate current display coordinate (where we're fetching from)
        uint16_t display_x;
        if (x_coord >= first_visible) {
            display_x = x_coord - first_visible;
        } else {
            display_x = (pixels_per_line - first_visible) + x_coord;
        }
        
        // Initialize xscroll when we reach the left border edge at the FETCH position
        // The pixels will be output 12 pixels later, but the sequencer state needs to be ready NOW
        if (display_x == vicii->border.border_left) {
            seq->xscroll_counter = vicii->registers.data[VICII_C2] & VICII_C2_XSCROLL;
            seq->pixel_in_char = 0;
        }

        // Reset shift register on mode change (but not x-scroll)
        if (seq->graphics_mode != seq->last_mode) {
            seq->last_mode = seq->graphics_mode;
            seq->shift_reg = 0;
        }
        // Use VMLI (Video Matrix Line Index) to determine which graphics data to sequence
        // According to vic-ii.txt documentation (lines 1161-1164), VMLI is a 6-bit counter
        // that tracks position within the internal 40×12 bit video matrix/color line.
        // The hardware increments VMLI after each g-access (line 1184), and the pixel
        // sequencer reads from the buffer position specified by VMLI.
        //
        // CRITICAL: We must use the vmli value from BEFORE the increment (stored in
        // current_vmli_for_display) because the graphics data was stored at that position,
        // and then vmli was incremented. The pixel sequencer needs to read from the position
        // that was just written.
        const uint8_t vmli = seq->current_vmli_for_display;
        
        // Load shift register from graphics line buffer at current VMLI position
        // Clamp to valid range (0-39) to prevent out-of-bounds access
        if (vmli < 40) {
            seq->shift_reg = seq->graphics_line[vmli];
        }
        
        // Sequence exactly 8 pixels from shift register
        for (int pixel = 0; pixel < 8; pixel++) {
            // Apply 12-pixel pipeline delay: data fetched at x_coord is displayed 12 pixels later
            const uint16_t pixel_x = x_coord + (uint16_t)pixel + VICII_PIPELINE_DELAY_PIXELS;
            
            vicii_pixel_t pixel_data;
            
            // XSCROLL handling - delay pixel output by XSCROLL pixels
            if (seq->xscroll_counter > 0) {
                seq->xscroll_counter--;
                pixel_data = seq->colors[0]; // Background during scroll delay
                vicii_pixel_emit_at_x(vicii, &pixel_data, pixel_x);
                continue;
            }
            
            // Extract pixel from shift register based on graphics mode
            uint8_t color_index = 0;
            uint8_t pixel_bits = 0;
            bool is_background = false;
            
            switch (seq->graphics_mode) {
                case VICII_GM_STANDARD_TEXT:
                    pixel_bits = (seq->shift_reg >> 7) & 1;
                    color_index = pixel_bits ?
                        (uint8_t)vicii->video_data.video_color_line[vmli] :
                        vicii->registers.data[VICII_B0C];
                    is_background = (pixel_bits == 0);
                    seq->shift_reg <<= 1;
                    break;
                    
                case VICII_GM_MULTICOLOR_TEXT:
                    if (vicii->video_data.video_color_line[vmli] & 0x08) {
                        // Multicolor character - 2 bits per pixel
                        pixel_bits = (seq->shift_reg >> 6) & 3;
                        switch (pixel_bits) {
                            case 0: color_index = vicii->registers.data[VICII_B0C]; break;
                            case 1: color_index = vicii->registers.data[VICII_B1C]; break;
                            case 2: color_index = vicii->registers.data[VICII_B2C]; break;
                            case 3: color_index = vicii->video_data.video_color_line[vmli]; break;
                        }
                        is_background = (pixel_bits == 0);
                        seq->shift_reg <<= 2;
                        seq->pixel_in_char = (seq->pixel_in_char + 2) & 7;
                    } else {
                        // Standard character in multicolor mode
                        pixel_bits = (seq->shift_reg >> 7) & 1;
                        color_index = pixel_bits ?
                            (uint8_t)vicii->video_data.video_color_line[vmli] :
                            vicii->registers.data[VICII_B0C];
                        is_background = (pixel_bits == 0);
                        seq->shift_reg <<= 1;
                        seq->pixel_in_char = (seq->pixel_in_char + 1) & 7;
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
                    pixel_bits = (seq->shift_reg >> 6) & 3;
                    switch (pixel_bits) {
                        case 0: color_index = vicii->registers.data[VICII_B0C]; break;
                        case 1: color_index = vicii->video_data.video_matrix_line[vmli] >> 4; break;
                        case 2: color_index = vicii->video_data.video_matrix_line[vmli] & 0x0F; break;
                        case 3: color_index = vicii->video_data.video_color_line[vmli]; break;
                    }
                    is_background = (pixel_bits == 0);
                    seq->shift_reg <<= 2;
                    seq->pixel_in_char = (seq->pixel_in_char + 2) & 7;
                    break;
                    
                case VICII_GM_ECM_TEXT:
                    pixel_bits = (seq->shift_reg >> 7) & 1;
                    if (pixel_bits) {
                        color_index = vicii->video_data.video_color_line[vmli];
                    } else {
                        const uint8_t bg_select = (vicii->video_data.video_matrix_line[vmli] >> 6) & 3;

                        color_index = vicii->registers.data[VICII_B0C + bg_select];
                    }
                    is_background = (pixel_bits == 0);
                    seq->shift_reg <<= 1;
                    break;
                    
                default:
                    color_index = vicii->registers.data[VICII_B0C];
                    is_background = true;
                    break;
            }
            
            // Set pixel data
            pixel_data.color = static_cast<vicii_color_t>(color_index);
            pixel_data.priority = is_background ? VICII_PRIORITY_BACKGROUND : VICII_PRIORITY_FOREGROUND;
            vicii_pixel_emit_at_x(vicii, &pixel_data, pixel_x);
            
            // Increment pixel position within character (for standard modes)
            if (((seq->graphics_mode & VICII_BITMAP_MODE_MASK) == 0) &&
                 !(vicii->video_data.video_color_line[vmli] & 0x08)) {
                seq->pixel_in_char = (seq->pixel_in_char + 1) & 7;
            }
        }
    }
    
    // Sprites are processed every cycle and can overlay any area
    // They have priority over both graphics and border pixels
    vicii_sprite_sequencer(vicii);
}

void vicii_pixel_flush_line(vicii_t* vicii, const uint32_t* palette, int y) {
    if (!vicii->pixel.framebuffer || !palette || y >= vicii->pixel.framebuffer_height) return;
    
    vicii_pixel_unit_t* pixel = &vicii->pixel;
    uint32_t* row_ptr = &pixel->framebuffer[y * pixel->framebuffer_width];
    const uint8_t border_color_index = vicii->registers.data[VICII_EC] & 0x0F;
    const uint32_t border_color = palette[border_color_index];
    
    // Fill entire line with border color first
    for (int x = 0; x < pixel->framebuffer_width; x++) {
        row_ptr[x] = border_color;
    }
    
    // VIC-II writes directly to its framebuffer without any offset.
    // The GUI layer handles centering by copying vicii_buffer (403x284)
    // to the centered position in screen_buffer (512x384).
    // VIC-II framebuffer_width should match visible_pixels_per_line (403 for PAL).
    if (pixel->pixel_line_color) {
        // Copy pixels from line buffer directly to framebuffer (no offset)
        for (int x = 0; x < vicii->config->visible_pixels_per_line && x < pixel->framebuffer_width; x++) {
            const uint8_t color_index = pixel->pixel_line_color[x] & 0x0F;
            row_ptr[x] = palette[color_index];
        }
    }
}

static inline void vicii_pixel_set_framebuffer(vicii_pixel_unit_t* pixel, uint32_t* framebuffer, 
                                             int width, int height) {
    pixel->framebuffer = framebuffer;
    pixel->framebuffer_width = width;
    pixel->framebuffer_height = height;
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

void vicii_memory_bank_change(void* chip, uint8_t bank) {
    vicii_t* vicii = (vicii_t*)chip;
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

static inline bus_state_t vicii_bus_memory_setup(vicii_t* vicii, bus_state_t bus_state, uint16_t address) {
    // Bank base offset applied here to keep operations in most appropriate place
    const uint16_t final_address = vicii->memory.bank_base | address;

    // Set up the address on the bus for the memory service phase to handle
    // This follows the same pattern as the CPU's bus_setup_read()
    BUS_SET_ADDR(bus_state, final_address);
    // TODO : Make sure RW line is always set (clear before and
    // set after CPU writes) so that we don't need to set it here
    BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) | BUS_MASK_RW); // Set read mode
    return bus_state;
}

// ========================================================================================
// SEQUENCER LOGIC
// ========================================================================================

static inline void vicii_sequencer_update_colors(vicii_t* vicii) {
    vicii_sequencer_unit_t* sequencer = &vicii->sequencer;
    vicii_registers_unit_t* regs = &vicii->registers;
    
    // Update color palette based on graphics mode and background colors
    switch (sequencer->graphics_mode) {
        case VICII_GM_STANDARD_TEXT:
            sequencer->colors[0].color = static_cast<vicii_color_t>(regs->data[VICII_B0C]);
            sequencer->colors[0].priority = VICII_PRIORITY_BACKGROUND;
            sequencer->colors[4].priority = VICII_PRIORITY_FOREGROUND;
            break;
        case VICII_GM_MULTICOLOR_TEXT:
            sequencer->colors[0].color = static_cast<vicii_color_t>(regs->data[VICII_B0C]);
            sequencer->colors[1].color = static_cast<vicii_color_t>(regs->data[VICII_B1C]);
            sequencer->colors[2].color = static_cast<vicii_color_t>(regs->data[VICII_B2C]);
            sequencer->colors[0].priority = VICII_PRIORITY_BACKGROUND;
            sequencer->colors[1].priority = VICII_PRIORITY_FOREGROUND;
            sequencer->colors[2].priority = VICII_PRIORITY_FOREGROUND;
            sequencer->colors[3].priority = VICII_PRIORITY_FOREGROUND;
            break;
        case VICII_GM_STANDARD_BITMAP:
            sequencer->colors[0].priority = VICII_PRIORITY_BACKGROUND;
            sequencer->colors[4].priority = VICII_PRIORITY_FOREGROUND;
            break;
        case VICII_GM_MULTICOLOR_BITMAP:
            sequencer->colors[0].color = static_cast<vicii_color_t>(regs->data[VICII_B0C]);
            sequencer->colors[0].priority = VICII_PRIORITY_BACKGROUND;
            sequencer->colors[1].priority = VICII_PRIORITY_FOREGROUND;
            sequencer->colors[2].priority = VICII_PRIORITY_FOREGROUND;
            sequencer->colors[3].priority = VICII_PRIORITY_FOREGROUND;
            break;
        case VICII_GM_ECM_TEXT:
            sequencer->colors[0].color = static_cast<vicii_color_t>(regs->data[VICII_B0C]);
            sequencer->colors[1].color = static_cast<vicii_color_t>(regs->data[VICII_B1C]);
            sequencer->colors[2].color = static_cast<vicii_color_t>(regs->data[VICII_B2C]);
            sequencer->colors[3].color = static_cast<vicii_color_t>(regs->data[VICII_B3C]);
            for (int i = 0; i < 4; i++) {
                sequencer->colors[i].priority = VICII_PRIORITY_BACKGROUND;
            }
            sequencer->colors[4].priority = VICII_PRIORITY_FOREGROUND;
            break;
        default:
            for (int i = 0; i < 5; i++) {
                sequencer->colors[i].color = VICII_COLOR_BLACK;
                sequencer->colors[i].priority = VICII_PRIORITY_BACKGROUND;
            }
            break;
    }
}

// ========================================================================================
// REGISTER HANDLING
// ========================================================================================

static inline void vicii_sequencer_update_mode(vicii_sequencer_unit_t* sequencer, uint8_t c1_reg, uint8_t c2_reg) {
    sequencer->graphics_mode = ((c1_reg & (VICII_C1_ECM | VICII_C1_BMM)) |
                               (c2_reg & VICII_C2_MCM)) >> 4;
}

// Timing functions
void vicii_update_badline_condition(vicii_t* vicii) {
    uint16_t raster = vicii->timing.raster_counter;
    
    // Bad lines only occur in range $30-$F7 (48-247)
    // Single range check instead of two comparisons
    if ((raster - 48) < 200) {  // Equivalent to raster >= 48 && raster < 248
        // "A Bad Line Condition is given at any arbitrary clock cycle, if at the
        // negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
        // <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
        // DEN bit was set during an arbitrary cycle of raster line $30."
        
        // Check if raster line $30 to capture DEN state
        if (raster == 0x30) {
            if (!vicii->video_logic.was_den_set_during_raster_30) {
                vicii->video_logic.was_den_set_during_raster_30 =
                    (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
            }
        }
        vicii->video_logic.is_bad_line = vicii->video_logic.was_den_set_during_raster_30 &&
                                 ((raster & 0x07) == (vicii->registers.data[VICII_C1] & VICII_C1_YSCROLL));
    } else {
        vicii->video_logic.is_bad_line = false;
    }
}

static inline void vicii_registers_write_interrupt(vicii_registers_unit_t* regs, uint8_t value) {
    // Only consider the 4 actually supported interrupt bits (IRST/IMBC/IMMC/ILP)
    value &= VICII_INTERRUPTS_MASK;
    // Fetch the current Interrupt Register value
    uint8_t ir = regs->data[VICII_IR];
    // Clear all '1' bits in the Interrupt Register
    ir &= ~value;
    // Always set the not-connected bits high
    ir |= VICII_IR_UNUSED;
    // Store the resulting bits
    regs->data[VICII_IR] = ir;
    // Note/TODO : Here, it's assumed that when all interrupt bits are cleared, the
    // IR_IRQ flag is untouched - it'll be cleared later, in vicii_handle_raster_interrupt()
}

// Register write function (uses all the above handlers)
bus_state_t vicii_registers_write(void* context, bus_state_t bus_state) {
    vicii_t* vicii = (vicii_t*)context;
    uint8_t value = BUS_GET_DATA(bus_state);
    uint8_t reg = BUS_GET_ADDR(bus_state) & VICII_REGS_MASK; // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
    // Notes:
    // * Some not-connected bits (marked with '-') are written anyway here,
    //   because determing the mask for those would only be slower, for no benefit
    //   (and these not-connected bits are turned into 1's in MaskBusRead anyway).
    // * Writes on 4 bit color registers ARE masked, to avoid having to do that in (often repeated) reads
    // * Instead of skipping writes to MxM and MxD, their reads are rerouted to MxM_2 and MxD_2
    // * Unused register indices 47..63 are written anyway here
    //   because avoiding those would only be slower, for no benefit

    if (reg == VICII_IR) { // $d019 Interrupt Register
        // Treat the latching Interrupt Register differently from the other registers
        vicii_registers_write_interrupt(&vicii->registers, value);
        return bus_state; // No further processing needed for IR
    }
    
    // Mask color registers to 4 bits
    if (reg >= VICII_EC) { // $d020 (4 bits) Exterior color (Border)
        // "When writing a color register ($D020-$D02E) currently being used to
        // display graphics a grey dot (color 15) appears at the first pixel of the
        // cycle. The reason for the grey dot appears to be a glitch in the color register
        // bank itself, not in the mapping from color enables to actual 4-bit color.
        // This effect is thus independent of the previous color register displayed."
        // Note: Grey dot effect only on 8565+ chips, implementation would require
        // cycle-exact tracking of which color registers are currently being displayed
        
        value &= 0x0F; // $d020 and up are colors - keep only lowest 4 bits
    }
    
    vicii->registers.data[reg] = value;
    
    // Unit-specific update handlers
    switch (reg) {
        case VICII_C1: // $d011 Control register 1
            // Update bad line condition when C1 changes (YSCROLL or DEN bit changes)
            vicii_update_badline_condition(vicii);
            FALLTHROUGH; // to C2 case
        case VICII_C2: // $d016 Control register 2
            vicii_sequencer_update_mode(&vicii->sequencer, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
            vicii_border_update_limits(&vicii->border, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
            break;
        case VICII_MXE: // $d015 Sprite enabled x
            // Update sprite enabled state
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
                sprite->enabled = (value & (1 << i)) != 0;
            }
            break;
        case VICII_MXYE: // $d017 Sprite Y expansion x
            // "Complex expansion flip flop logic per VIC-II documentation"
            // Optimized: set flip-flop state directly based on bit value
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
                // Writing 0 sets flip-flop, writing 1 clears it (immediate effect)
                // Note: cycle 55 inversion is handled separately in vicii_cycle()
                sprite->expansion_flip_flop = !(value & (1 << i));
            }
            break;
        case VICII_MP: // AI $d018 Memory pointers
            vicii_memory_update_mapping(&vicii->memory, value);
            break;
        case VICII_MXDP: // $d01b Sprite data priority
            // Batch update sprite priorities
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
                sprite->priority = (value & (1 << i)) ? 
                    VICII_PRIORITY_SPRITE_BEHIND : VICII_PRIORITY_SPRITE_IN_FRONT;
            }
            break;
        case VICII_EC: // $d020 (4 bits) Exterior color (Border)
            vicii->border.border_pixel.color = static_cast<vicii_color_t>(value); // value already masked to 0x0F above
            FALLTHROUGH; // to B0C-B2C case
        case VICII_B0C: // $d021 (4 bits) Background color 0
        case VICII_B1C: // $d022 (4 bits) Background color 1
        case VICII_B2C: // $d023 (4 bits) Background color 2
            // Inline vicii_border_update_color since priority is set only once during init
            vicii_sequencer_update_colors(vicii);
            break;
        case VICII_MM0: // $d025 (4 bits) Sprite multicolor 0
        case VICII_MM1: // $d026 (4 bits) Sprite multicolor 1
            // Global sprite multicolor registers - update all sprites with pre-masked value
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                // MM0/MM1 are global multicolor registers, not per-sprite
            }
            break;
        default:
            // No special handling needed for other registers
            break;
    }
    return bus_state;
}

// Register read helper (used by register read)
static inline uint8_t vicii_read_clear(vicii_registers_unit_t* regs, uint8_t reg) {
    uint8_t val = regs->data[reg];
    regs->data[reg] = 0;
    return val;
}

// Register read function (uses the above helper)
bus_state_t vicii_registers_read(void* context, bus_state_t bus_state) {
    vicii_t* vicii = (vicii_t*)context;
    uint16_t address = BUS_GET_ADDR(bus_state);
    uint8_t reg = address & VICII_REGS_MASK;
    // Used for "floating" bus state for subsequent unattached reads
    uint8_t data = BUS_GET_DATA(bus_state);
    
    // Fast path for most common registers
    switch (reg) {
        case VICII_C1:
            data = (vicii->registers.data[VICII_C1] & 0x7F) |       //    17 $d011 Control register 1
                   ((vicii->timing.raster_counter >> 1) & VICII_C1_RST8); //         bit 7 (RST8) reflects raster_counter bit 8
            break;
        case VICII_RASTER:
            data = vicii->timing.raster_counter & 0xFF;               //    18 $d012 Reflects raster_counter bits 0..7
            break;
        case VICII_C2:
            data = vicii->registers.data[VICII_C2] | (data & VICII_C2_UNUSED); //    22 $d016 |  - |  - | RES| MCM|CSEL|    XSCROLL   | Control register 2
            break;
        case VICII_MP:
            data = vicii->registers.data[VICII_MP] | (data & VICII_MP_UNUSED); //    24 $d018 |VM13|VM12|VM11|VM10|CB13|CB12|CB11|  - | Memory pointers
            break;
        case VICII_IR:
            data = vicii->registers.data[VICII_IR] | (data & VICII_IR_UNUSED); //    25 $d019 | IRQ|  - |  - |  - | ILP|IMMC|IMBC|IRST| Interrupt register
            break;
        case VICII_IE:
            data = vicii->registers.data[VICII_IE] | (data & VICII_IE_UNUSED); //    26 $d01a |  - |  - |  - |  - | ELP|EMMC|EMBC|ERST| Interrupt Enabled
            break;
        case VICII_MXM:
            data = vicii_read_clear(&vicii->registers, VICII_MXM_2);  //    30 $d01e Sprite-sprite collision is cleared on read
            break;
        case VICII_MXD:
            data = vicii_read_clear(&vicii->registers, VICII_MXD_2);  //    31 $d01f Sprite-data collision is cleared on read
            break;
        default:
            if (reg <= 29) {
                data = vicii->registers.data[reg];                  //  0-29 $d000-$d01f (except 22,24,25,26) use all 8 bits
            } else if (reg <= 46) {
                data = vicii->registers.data[reg] | (data & 0xF0);  // 32-46 $d020-$d02e use bits 0..3 (bits 4..7 are not connected)
            } else {
                data = data;                                        // 47-63 $d02f-$d03f unattached registers (many docs say: give $ff on reading)
            }
            break;
    }

    BUS_SET_DATA(bus_state, data);
    return bus_state;
}

// ========================================================================================
// TIMING AND VIDEO LOGIC
// ========================================================================================

void vicii_set_x_cycle(vicii_t* vicii, uint8_t value) {
    vicii->timing.x_cycle = value;
    
    // Update X coordinate (sprite/lightpen coordinate system)
    // X coordinate advances by 8 pixels per cycle (8 pixels displayed per cycle)
    // Documentation: "X coordinate 0" occurs halfway between cycles 13 and 14
    // The x_coordinate wraps at pixels_per_line (504 for PAL, 520 for NTSC)
    // Formula derived from vic-ii.txt timing diagram (lines 989-992):
    // - Cycle 13 start: x_coordinate = 0x1F4 (500)
    // - Cycle 14 start: x_coordinate = 0x004 (4)
    // - This requires: (base_offset + cycle*8) mod pixels_per_line
    const uint16_t pixels_per_line = vicii->config->cycles_per_line * 8;
    vicii->timing.x_coordinate = (vicii->config->first_x_coord + (vicii->timing.x_cycle * 8)) % pixels_per_line;
}

// Helper function: Reset VCBASE/VC when outside display area
// Called both when entering line 0 and throughout lines outside $30-$F7
static inline void vicii_reset_vcbase_vc(vicii_t* vicii) {
    vicii->video_logic.vcbase = 0;
    vicii->video_logic.vc = 0;
}

// Helper function: Perform line 0 raster/IRQ operations
// Documentation (vic-ii.txt lines 1006-1010, 1012-1014):
// "Raster line 0 is, however, an exception: In this line, IRQ and incrementing
// (resp. resetting) of RASTER are performed one cycle later than in the other lines."
//
// This function handles both immediate (NTSC) and delayed (PAL) execution
static inline void vicii_perform_line0_raster_irq_operations(vicii_t* vicii) {
    // Reset raster counter to 0
    vicii->timing.raster_counter = 0;
    
    // Reset per-frame state
    vicii->video_logic.was_den_set_during_raster_30 = false;
    vicii->video_logic.is_bad_line = false;
    vicii->video_logic.refresh_counter = 0xFF;
    
    // Reset VCBASE/VC (shared with timing advance logic)
    vicii_reset_vcbase_vc(vicii);
    
    // TODO: Trigger raster IRQ here if enabled and raster compare == 0
}

void vicii_timing_advance(vicii_t* vicii) {
    const bool end_of_line = vicii->timing.x_cycle == vicii->config->cycles_per_line - 1;

    if (!end_of_line) {
        // Advance cycle counter
        vicii_set_x_cycle(vicii, vicii->timing.x_cycle + 1);
    } else {
        // CRITICAL: Flush the completed scanline BEFORE advancing to the next line
        // This ensures pixels from the PREVIOUS raster line are written to the framebuffer
        // at the correct Y position (which is still the OLD raster_counter value)
        const uint16_t completed_raster = vicii->timing.raster_counter;
        if (vicii->pixel.framebuffer && completed_raster < vicii->pixel.framebuffer_height) {
            vicii_pixel_flush_line(vicii, vicii_get_default_palette(), completed_raster);
        }
        
        vicii_set_x_cycle(vicii, 0);
        
        // CRITICAL: Check if we're about to enter raster 0x30 (first display raster)
        // If so, reset VCBASE, VC, and display_state BEFORE advancing the raster counter
        // This prevents cycle 58 on the previous line from corrupting VCBASE/VC
        // and ensures display_state starts FALSE (will be set TRUE by first bad line)
        if (vicii->timing.raster_counter == 0x2F) {
            vicii->video_logic.vcbase = 0;
            vicii->video_logic.vc = 0;
            vicii->video_logic.display_state = false;
        }
        
        // Normal line transition: update raster_counter
        if (++vicii->timing.raster_counter >= vicii->config->total_lines) {
            // Line wrap happens here, but line 0 operations are handled by cycle wrappers
            // (PAL: cycle 1, NTSC: cycle 0) which reset raster_counter, per-frame state, etc.
            // Just wrap the counter here - the cycle wrappers will handle the rest
            vicii->timing.raster_counter = vicii->config->total_lines - 1;
        }
        
        // Reset VCBASE/VC when outside display area ($30-$F7)
        if (vicii->timing.raster_counter < 0x30 || vicii->timing.raster_counter > 0xf7) {
            vicii_reset_vcbase_vc(vicii);
        }
        
        // Initialize with current border color from EC register
        if (vicii->pixel.pixel_line_color && vicii->config->visible_pixels_per_line > 0) {
            const uint8_t border_color = vicii->registers.data[VICII_EC] & 0x0F;
            memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, vicii->config->visible_pixels_per_line);
            memset(vicii->pixel.pixel_line_color, border_color, vicii->config->visible_pixels_per_line);
        }
    }
}

// ========================================================================================
// CYCLE FUNCTIONS
// ========================================================================================

static uint8_t vicii_cycle_sprite_p_access(vicii_t* vicii, int param_sprite_num) {
    bool den_enabled = (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
    
    // BA/AEC will be set centrally in vicii_tick based on access type
    if (den_enabled) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[param_sprite_num];

        if (sprite->enabled) { 
            // Set sprite pointer for next cycle 
            vicii->bus.active_sprite = sprite;
            return VIC_ACCESS_P;
        }
    }
    vicii->bus.active_sprite = NULL;
    return VIC_ACCESS_IDLE;
}

static uint8_t vicii_cycle_sprite_s_access(vicii_t* vicii, int param_sprite_num) {
    bool den_enabled = (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
    
    // BA/AEC will be set centrally in vicii_tick based on access type
    if (den_enabled) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[param_sprite_num];

        if (sprite->enabled) {
            // Set sprite pointer for next cycle 
            vicii->bus.active_sprite = sprite;
            return VIC_ACCESS_S;
        }
    }
    vicii->bus.active_sprite = NULL;
    return VIC_ACCESS_IDLE;
}

static uint8_t vicii_cycle_refresh(vicii_t* vicii, int unused_param) {
    bool den_enabled = (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
    
    // BA/AEC will be set centrally in vicii_tick based on access type
    if (den_enabled) {
        return VIC_ACCESS_REFRESH;
    } else {
        return VIC_ACCESS_IDLE;
    }
}

static uint8_t vicii_cycle_vc_load(vicii_t* vicii, int unused_param) {
    vicii->video_logic.vc = vicii->video_logic.vcbase;
    vicii->video_logic.vmli = 0;
    bool den_enabled = (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
    
    // BA/AEC will be set centrally in vicii_tick based on access type
    // On bad lines: set display_state = true and reset RC to 0
    // On non-bad lines: DO NOT change display_state (it's managed by cycle 58)
    if (vicii->video_logic.is_bad_line && den_enabled) {
        vicii->video_logic.display_state = true;
        vicii->video_logic.rc = 0;
    }

    // Note: display_state is NOT set to false here - that only happens in cycle 58 when RC==7
    return VIC_ACCESS_IDLE;
}

static uint8_t vicii_cycle_char_color_access(vicii_t* vicii, int unused_param_vmli) {
    bool den_enabled = (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
    
    // BA/AEC will be set centrally in vicii_tick based on access type
    if (den_enabled) {
        if (vicii->video_logic.is_bad_line) {
            return VIC_ACCESS_C;
        } else if (vicii->video_logic.display_state) {
            // On non-bad lines during display state, still need G-access for graphics data
            // DO NOT change display_state here - it's managed by cycle 15 (bad lines set it true)
            // and cycle 58 (RC==7 check sets it false when going to idle)
            return VIC_ACCESS_G;
        }
    }
    return VIC_ACCESS_IDLE;
}

static uint8_t vicii_cycle_idle(vicii_t* vicii, int unused_param) {
    // Idle cycle: VIC accesses during PHI1, CPU can use PHI2
    // BA/AEC will be set centrally in vicii_tick based on look-ahead
    return VIC_ACCESS_IDLE;
}

// PAL Cycle 1 wrapper: Execute line 0 operations here (delayed from cycle 0)
// Documentation (vic-ii.txt lines 1006-1010): "Raster line 0 is, however, an exception:
// In this line, IRQ and incrementing (resp. resetting) of RASTER are performed one cycle
// later than in the other lines." On PAL, cycle 0 just does sprite P-access (no operations).
static uint8_t vicii_cycle_sprite_s_1_pal(vicii_t* vicii, int param) {
    // IMPORTANT: Cycle functions execute BEFORE vicii_timing_advance(), so raster_counter
    // still contains the PREVIOUS line number. When we're on the last line (311 for PAL),
    // we know the NEXT line will be line 0, so we perform the line 0 operations here.
    const bool transitioning_to_line0 = (vicii->timing.raster_counter == vicii->config->total_lines - 1);
    
    if (transitioning_to_line0) {
        // Execute line 0 raster/IRQ operations (delayed by one cycle on PAL)
        vicii_perform_line0_raster_irq_operations(vicii);
    }
    
    // Call underlying cycle function (sprite 3 S-access for PAL)
    return vicii_cycle_sprite_s_access(vicii, param);
}

// NTSC Cycle 0 wrapper: Execute raster/IRQ operations immediately (no delay)
static uint8_t vicii_cycle_sprite_p_0_ntsc(vicii_t* vicii, int param) {
    // Check if we're transitioning into line 0
    const bool transitioning_to_line0 = (vicii->timing.raster_counter == vicii->config->total_lines - 1);
    
    if (transitioning_to_line0) {
        // NTSC: Execute immediately, no delay
        vicii_perform_line0_raster_irq_operations(vicii);
    }
    
    // Call underlying cycle function (sprite 3 P-access for NTSC)
    return vicii_cycle_sprite_p_access(vicii, param);
}

// Cycle 15: MCBASE increment when expansion flip-flop is set (Rule 7)
static inline void vicii_cycle_15_mcbase_expansion(vicii_t* vicii) {
    // "7. In the first phase of cycle 15, it is checked if the expansion flip flop
    // is set. If so, MCBASE is incremented by 2."
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
        if (sprite->expansion_flip_flop) {
            sprite->mcbase += 2;
        }
    }
}

// Cycle 16: Expansion flip-flop inversion and MCBASE increment (Rule 8)
static inline void vicii_cycle_16_expansion_check(vicii_t* vicii) {
    // "8. In the first phase of cycle 16, it is checked if the expansion flip flop
    // is set. If so, MCBASE is incremented by 1. After that, the VIC checks if
    // MCBASE is equal to 63 and turns off the DMA and the display of the sprite
    // if it is."
    //
    // Note: The documentation also mentions sprite crunch logic where if CPU cleared
    // the Y expansion bit in $d017 during cycle 15 PHI2, a different formula is used:
    // MCBASE = (0xAA & (MCBASE & MC)) | (0x55 & (MCBASE | MC))
    
    uint8_t mxye_reg = vicii->registers.data[VICII_MXYE];
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
        
        // First: If MxYE bit is set, invert the expansion flip-flop
        if (mxye_reg & (1 << i)) {
            sprite->expansion_flip_flop = !sprite->expansion_flip_flop;
        }
        // Second: If expansion flip-flop is set, increment MCBASE by 1
        if (sprite->expansion_flip_flop) {
            sprite->mcbase += 1;
        }
        // Third: Check if MCBASE == 63 and disable DMA if so
        if (sprite->mcbase == 63) {
            sprite->dma_enabled = false;
            sprite->display_state = false;
        }
    }
}

// Wrapper for cycle 15: VC load + MCBASE expansion check
static uint8_t vicii_cycle_vc_load_mcbase(vicii_t* vicii, int unused_param) {
    vicii_cycle_15_mcbase_expansion(vicii);
    return vicii_cycle_vc_load(vicii, unused_param);
}

// Wrapper for cycle 16: c/g access + expansion flip-flop check
static uint8_t vicii_cycle_char_color_expansion_check(vicii_t* vicii, int unused_param_vmli) {
    vicii_cycle_16_expansion_check(vicii);
    return vicii_cycle_char_color_access(vicii, unused_param_vmli);
}

// Helper: Sprite Y-coordinate matching (shared by cycles 55 and 56)
static void vicii_sprite_y_coordinate_check(vicii_t* vicii, bool is_cycle_55) {
    // "2. If the MxYE bit is set in the first phase of cycle 55, the expansion
    // flip flop is inverted."
    // "3. In the first phases of cycle 55 and 56, the VIC checks for every sprite
    // if the corresponding MxE bit in register $d015 is set and the Y coordinate
    // of the sprite (odd registers $d001-$d00f) match the lower 8 bits of RASTER.
    // If this is the case and the DMA for the sprite is still off, the DMA is
    // switched on, MCBASE is cleared, and if the MxYE bit is set the expansion
    // flip flip is reset."
    
    uint8_t mxe_reg = vicii->registers.data[VICII_MXE];
    uint8_t mxye_reg = vicii->registers.data[VICII_MXYE];
    uint16_t raster = vicii->timing.raster_counter;
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
        
        // Cycle 55 only: Invert expansion flip-flop if MxYE bit is set
        if (is_cycle_55 && (mxye_reg & (1 << i))) {
            sprite->expansion_flip_flop = !sprite->expansion_flip_flop;
        }        
        // Both cycles 55 and 56: Check Y-coordinate match
        if ((mxe_reg & (1 << i)) && !sprite->dma_enabled) {
            uint8_t sprite_y = vicii->registers.data[VICII_M0Y + i * 2];
            if ((raster & 0xFF) == sprite_y) {
                // Enable DMA, clear MCBASE, reset expansion flip-flop if MxYE set
                sprite->dma_enabled = true;
                sprite->mcbase = 0;
                if (mxye_reg & (1 << i)) {
                    sprite->expansion_flip_flop = false;
                }
            }
        }
    }
}

// Cycle 55: Sprite Y-match + c/g access
static uint8_t vicii_cycle_char_color_y_match(vicii_t* vicii, int unused_param_vmli) {
    vicii_sprite_y_coordinate_check(vicii, true);
    return vicii_cycle_char_color_access(vicii, unused_param_vmli);
}

// Cycle 56: Sprite Y-match + idle access
static uint8_t vicii_cycle_idle_y_match(vicii_t* vicii, int unused_param) {
    vicii_sprite_y_coordinate_check(vicii, false);
    return vicii_cycle_idle(vicii, unused_param);
}

// Cycle 58: RC check and VCBASE update (Rule 5 from Section 3.7.2)
static inline void vicii_cycle_58_rc_check(vicii_t* vicii) {
    // "In the first phase of cycle 58, the VIC checks if RC=7. If so, the video
    // logic goes to idle state and VCBASE is loaded from VC (VC->VCBASE). If
    // "If the video logic is in display state afterwards (this is always the case
    // if there is a Bad Line Condition), RC is incremented."
    
    // "If the video logic is in display state... RC is incremented."
    // Check display_state BEFORE potentially changing it
    bool should_increment_rc = vicii->video_logic.display_state;
    
    if (vicii->video_logic.rc == 7) {
        // "The transition from display to idle state occurs in cycle 58 of a line
        // if the RC contains the value 7 and there is no Bad Line Condition."
        if (!vicii->video_logic.is_bad_line) {
            vicii->video_logic.display_state = false;
            vicii->video_logic.vcbase = vicii->video_logic.vc;
            should_increment_rc = false; // Override: don't increment when going to idle
        }
    }
    // Increment RC if we determined we should
    if (should_increment_rc) {
        vicii->video_logic.rc = (vicii->video_logic.rc + 1) & 0x07;
    }

    // Rule 4: "In the first phase of cycle 58, the MC of every sprite is loaded from
    // its belonging MCBASE (MCBASE->MC) and it is checked if the DMA for the
    // sprite is turned on and the Y coordinate of the sprite matches the lower
    // 8 bits of RASTER. If this is the case, the display of the sprite is
    // turned on."    
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
        
        // Load MC from MCBASE
        sprite->mc = sprite->mcbase;        
        // Check if we should turn on sprite display
        if (sprite->dma_enabled && !sprite->display_state) {
            uint8_t sprite_y = vicii->registers.data[VICII_M0Y + i * 2];

            if ((vicii->timing.raster_counter & 0xFF) == sprite_y) {
                sprite->display_state = true;
            }
        }
    }
}

// Cycle 58: RC check and VCBASE update, then sprite S access with MC load
// Combines Rule 5 (Section 3.7.2) and Rule 4 (Section 3.8.1)
static uint8_t vicii_cycle_sprite_p_rc_mc_load(vicii_t* vicii, int param_sprite_num) {
    vicii_cycle_58_rc_check(vicii);
    return vicii_cycle_sprite_p_access(vicii, param_sprite_num);
}

// Border Rules 2 & 3: Y coordinate checks in cycle 63 (1-based numbering)
// Combined with sprite S access for cycle efficiency
static inline void vicii_cycle_63_border_check(vicii_t* vicii) {
    uint16_t raster = vicii->timing.raster_counter;
    bool den_set = (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
                
    // Rule 2: "If the Y coordinate reaches the bottom comparison value in cycle 63,
    // the vertical border flip flop is set."
    if (raster == vicii->border.border_bottom) {
        vicii->border.vertical_border_flip_flop = true;
    }

    // Rule 3: "If the Y coordinate reaches the top comparison value in cycle 63 and
    // the DEN bit in register $d011 is set, the vertical border flip flop is reset."
    else if (raster == vicii->border.border_top && den_set) {
        vicii->border.vertical_border_flip_flop = false;
    }    
}

static uint8_t vicii_cycle_sprite_s_border_check(vicii_t* vicii, int param_sprite_num) {
    vicii_cycle_63_border_check(vicii);
    // Perform the sprite S access for this cycle
    return vicii_cycle_sprite_s_access(vicii, param_sprite_num);
}
// Border flip-flop logic (Documentation section 3.9) - X coordinate rules only
// Border limits work in a simple linear coordinate system starting from first visible pixel
static inline void vicii_border_update_flip_flops_x(vicii_border_unit_t* border,
        vicii_timing_unit_t* timing, uint8_t c1_reg, const vicii_chip_config_t* config) {
    const uint16_t raster = timing->raster_counter;
    const uint16_t x_coord = timing->x_coordinate;
    const bool den_set = (c1_reg & VICII_C1_DEN) != 0;
    
    // Convert x_coordinate to simple linear display coordinate (0-402 for PAL)
    // This matches how vicii_pixel_emit_at_x() maps coordinates to the line buffer
    const uint16_t pixels_per_line = config->cycles_per_line * 8;
    const uint16_t first_visible = config->first_visible_x_coord;
    
    // Check each pixel in this cycle (8 pixels)
    for (int pixel = 0; pixel < 8; pixel++) {
        const uint16_t pixel_x_coord = (x_coord + (uint16_t)pixel) % pixels_per_line;
        
        // Convert to display coordinate (0-402)
        uint16_t display_x;
        if (pixel_x_coord >= first_visible) {
            display_x = pixel_x_coord - first_visible;
        } else {
            display_x = (pixels_per_line - first_visible) + pixel_x_coord;
        }
        
        // Skip if outside visible range
        if (display_x >= config->visible_pixels_per_line) {
            continue;
        }
        
        // Rule 1: "If the X coordinate reaches the right comparison value, the main border flip flop is set."
        if (display_x == border->border_right) {
            border->main_border_flip_flop = true;
        }
        
        // Rules 4, 5, 6: Handle both flip-flops at border_left
        if (display_x == border->border_left) {
            // Rule 4: Set vertical flip-flop if at bottom border
            if (raster == border->border_bottom) {
                border->vertical_border_flip_flop = true;
            }
            // Rule 5: Clear vertical flip-flop if at top border with DEN set
            else if (raster == border->border_top && den_set) {
                border->vertical_border_flip_flop = false;
            }
            
            // Rule 6: Clear main flip-flop when vertical flip-flop is clear
            if (!border->vertical_border_flip_flop) {
                border->main_border_flip_flop = false;
            }
        }
    }
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

static inline bus_state_t vicii_bus_control_aec_high(bus_state_t bus_state) {
    BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) | BUS_MASK_AEC);
    return bus_state;
}

static inline bus_state_t vicii_bus_control_aec_low(bus_state_t bus_state) {
    BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) & ~BUS_MASK_AEC);
    return bus_state;
}

static inline bus_state_t vicii_bus_control_ba_high(bus_state_t bus_state) {
    // BA HIGH: Bus is available to CPU during PHI2
    // This is the normal/default state
    BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) | BUS_MASK_BA);
    return bus_state;
}

static inline bus_state_t vicii_bus_control_ba_low(bus_state_t bus_state) {
    // BA LOW: VIC will need PHI2 bus access (prevents CPU from accessing bus)
    // This happens during:
    // - Bad Line c-accesses (character pointer reads)
    // - Sprite p-accesses (sprite data pointer reads)
    // - Sprite s-accesses (sprite data reads)
    BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) & ~BUS_MASK_BA);
    return bus_state;
}

// ========================================================================================
// CENTRALIZED BUS CONTROL LOGIC
// ========================================================================================

// Check if a specific cycle needs PHI2 bus access (c/p/s access)
// Uses cycle number ranges and cycle table param field for sprite accesses
static inline bool vicii_future_cycle_needs_phi2_access(vicii_t* vicii, uint8_t cycle) {
    if (cycle >= vicii->config->cycles_per_line) {
        return false;
    }
    
    bool den_enabled = (vicii->registers.data[VICII_C1] & VICII_C1_DEN) != 0;
    if (!den_enabled) {
        return false;
    }
    
    // Check bad line c-access range (cycles 15-54 for PAL, same for NTSC)
    if (cycle >= 15 && cycle <= 54) {
        // CRITICAL FIX: Can't rely on is_bad_line for future cycles because it might not be updated yet
        // Instead, manually check if the CURRENT raster + any wrap to next line will be a bad line
        // Bad lines occur when: raster >= 0x30 && raster <= 0xF7 && (raster & 0x07) == YSCROLL
        uint16_t check_raster = vicii->timing.raster_counter;
        
        // If checking a cycle that's earlier than current cycle, we're looking at next line
        if (cycle < vicii->timing.x_cycle) {
            check_raster = (check_raster + 1) % vicii->config->total_lines;
        }
        
        // Check bad line condition for the target raster
        if ((check_raster - 48) < 200) {  // Equivalent to: check_raster >= 48 && check_raster < 248
            uint8_t yscroll = vicii->registers.data[VICII_C1] & VICII_C1_YSCROLL;
            bool was_den_set = vicii->video_logic.was_den_set_during_raster_30;
            return was_den_set && ((check_raster & 0x07) == yscroll);
        }
        return false;
    }
    
    // Get sprite number from cycle table param field
    const vicii_cycle_entry_t* entry = &vicii->timing.cycle_table[cycle];
    int sprite_num = entry->param; // -1 for non-sprite accesses
    
    if (sprite_num >= 0 && sprite_num < VICII_NUM_SPRITES) {
        return vicii->sprites.sprites[sprite_num].enabled;
    }
    
    return false;
}

// Centralized function to set BA/AEC based on current and future cycle needs
// This should be called once per cycle in vicii_tick
static inline bus_state_t vicii_update_ba_aec_signals(vicii_t* vicii, bus_state_t bus_state, uint8_t access_type) {
    uint8_t current_cycle = vicii->timing.x_cycle;
    
    // Check if we need PHI2 access NOW (current cycle)
    bool needs_phi2_now = (access_type == VIC_ACCESS_C ||
                           access_type == VIC_ACCESS_P ||
                           access_type == VIC_ACCESS_S);
    
    if (needs_phi2_now) {
        // Current cycle needs PHI2 access: BA LOW, AEC LOW
        bus_state = vicii_bus_control_ba_low(bus_state);
        bus_state = vicii_bus_control_aec_low(bus_state);
        return bus_state;
    }
    
    // Check if we'll need PHI2 access in 3 cycles (BA goes low 3 cycles before VIC needs bus)
    uint8_t future_cycle = (current_cycle + 3) % vicii->config->cycles_per_line;
    bool needs_phi2_future = vicii_future_cycle_needs_phi2_access(vicii, future_cycle);
    
    if (needs_phi2_future) {
        // Future cycle needs PHI2 access: BA LOW (warning), AEC HIGH
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
// MAIN CYCLE FUNCTION
// ========================================================================================

// Main tick function
bus_state_t vicii_tick(vicii_t* vicii, bus_state_t bus_state) {
    c64_bus_t* c64_bus = (c64_bus_t*)vicii->bus.bus;
    // STEP 1: Read data from bus (from PREVIOUS cycle's PHI2 memory setup)
    uint8_t bus_data = BUS_GET_DATA(c64_bus->state);
    // VIC-II bank switching is now handled via a callback registered with CIA2
    // The callback is set up during C64 system initialization in c64.cpp
    // This keeps VIC-II and CIA implementations decoupled
    // Handle the read data based on the pending access type
    // This handles C/P/S accesses that were set up at the end of the previous cycle
    switch (vicii->bus.pending_phi2_access_type) {
        case VIC_ACCESS_P:
            // P-access: Store sprite pointer
            if (vicii->bus.active_sprite) {
                vicii->bus.active_sprite->data_pointer = bus_data;
                // Clear pending access
                vicii->bus.active_sprite = NULL;
            }
            break;
        case VIC_ACCESS_S:
            // S-access: Store sprite data
            if (vicii->bus.active_sprite) {
                uint mc = vicii->bus.active_sprite->mc_counter;

                if (mc < 3) {
                    // Store data in shift register at appropriate position
                    uint32_t shift_data = (uint32_t)bus_data << (16 - mc * 8);
                    vicii->bus.active_sprite->shift_reg |= shift_data;
                }
                // Increment mc_counter after storing the data
                vicii->bus.active_sprite->mc_counter++;
                // Clear pending access
                vicii->bus.active_sprite = NULL;
            }
            break;
        case VIC_ACCESS_C:
            // C-access: Store video matrix data that arrived from PREVIOUS cycle's PHI2 setup
            // ONLY happens on bad lines
            if (vicii->video_logic.display_state && vicii->video_logic.vmli < 40) {
                vicii->video_data.video_matrix_line[vicii->video_logic.vmli] = bus_data;
                // Increment VC and VMLI after storing c-access data
                vicii->video_logic.vc++;
                vicii->video_logic.vmli++;
            }
            break;
        default: // VIC_ACCESS_IDLE, VIC_ACCESS_REFRESH, VIC_ACCESS_G
            // G-access: No video matrix storage, just graphics data read
            // This happens on non-bad lines during display_state
            break;
    }
    
    // Clear pending access
    vicii->bus.pending_phi2_access_type = VIC_ACCESS_IDLE;

    // CRITICAL: Update bad line condition BEFORE calling cycle function
    // The cycle function needs current bad line status to return correct access type
    vicii_update_badline_condition(vicii);

    // STEP 2: Get current cycle entry and parameter
    const vicii_cycle_entry_t* entry = &vicii->timing.cycle_table[vicii->timing.x_cycle];
    const int access_param = entry->param;

    // Call cycle function to determine current access type, returning either
    // VIC_ACCESS_IDLE, VIC_ACCESS_REFRESH, VIC_ACCESS_P, VIC_ACCESS_S, or VIC_ACCESS_C
    const uint8_t access_type = entry->func(vicii, access_param);

    // CRITICAL FIX: Update BA/AEC signals at the START of the cycle (PHI1 phase)
    // This must happen BEFORE the CPU's PHI2 tick so the CPU sees the correct BA state.
    // BA must go low 3 cycles BEFORE VIC needs PHI2 access, and this look-ahead
    // must be calculated at the cycle boundary, not at the end of the cycle.
    // Moving this here from the end of vicii_tick() fixes the half-cycle timing error
    // that was causing screen corruption during bad lines and sprite DMA.
    bus_state = vicii_update_ba_aec_signals(vicii, bus_state, access_type);

    // STEP 3: Perform PHI1 memory accesses via direct read
    // G-access happens EVERY cycle, other accesses are special cases
    uint16_t address;
    
    // Now calculate address for PHI1 read based on cycle type
    switch (access_type) {
        case VIC_ACCESS_REFRESH:
            // Refresh cycles use special address
            address = vicii->memory.vm_base | 0x3F00 | vicii->video_logic.refresh_counter;
            vicii->video_logic.refresh_counter--;
            break;
        case VIC_ACCESS_C:
            // C-access: Read Color RAM during PHI1 (happens on bad lines only)
            BUS_SET_ADDR(bus_state, vicii->memory.vm_base | vicii->video_logic.vc);
            bus_state = mos2114_read(vicii->colorram, bus_state);
            // Store at current vmli position
            if (vicii->video_logic.display_state && vicii->video_logic.vmli < 40) {
                const uint8_t color_data = BUS_GET_DATA(bus_state) & 0x0F;
                vicii->video_data.video_color_line[vicii->video_logic.vmli] = static_cast<vicii_color_t>(color_data);
            }
            // Fall through to G-access
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
                if (vicii->sequencer.graphics_mode & VICII_BITMAP_MODE_MASK) {
                    // Bitmap mode: CB13 provides bit 13, VC provides bits 3-12, RC provides bits 0-2
                    // Documentation section 3.7.3.3, line 1394: |CB13| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0| RC2| RC1| RC0|
                    // Use VCBASE + VMLI to get the VC value for this column position
                    const uint16_t vc_for_column = (vicii->video_logic.vcbase + vmli) & 0x3FF;
                    const uint16_t cb13_bit = vicii->memory.cb_base & (1 << 13);

                    address = cb13_bit | (vc_for_column << 3);
                } else {
                    // Text mode: address uses character code from video matrix
                    const uint8_t char_code = vicii->video_data.video_matrix_line[vmli];

                    address = vicii->memory.cb_base | (char_code << 3);
                }
                address |= vicii->video_logic.rc; // Add row counter (3 bits)
                break;
            }
            // Fall through to idle if display_state is false
            FALLTHROUGH;
        default: // VIC_ACCESS_IDLE, VIC_ACCESS_P, VIC_ACCESS_S
            // Idle, p-access, s-access OR display_state==false: Use idle address
            address = 0x3fff;
            break;
    }

    // If Extended Color Mode (ECM) bit is set, hold address lines 9 and 10 low
    if (vicii->sequencer.graphics_mode & VICII_EXTENDED_COLOR_MODE_MASK) {
        address &= ~(0x03 << 9);
    }

    // Apply CIA2 originating vic-ii bank base (set in vicii_memory_bank_change)
    address |= vicii->memory.bank_base;

    // Perform PHI1 memory read (common path for all PHI1 accesses)
    bus_state = c64_bus_vic_read(c64_bus, bus_state, address);
    // STEP 4: Load graphics data into line buffer during cycles 15-54 (0-indexed)
    // During display_state, we need graphics data for every raster line (not just bad lines)
    // to show different rows of each character
    if (vicii->video_logic.display_state && vicii->timing.x_cycle >= 15 && vicii->timing.x_cycle <= 54) {
        // G-access happens EVERY cycle during PHI1 (the address calculation above always runs)
        // The graphics sequencer will use the data when in display_state
        // Use VMLI hardware register for column position (matches hardware behavior)
        const uint8_t graphics_data = BUS_GET_DATA(bus_state);
        const uint8_t vmli = vicii->video_logic.vmli;

        vicii_graphics_sequencer(vicii, graphics_data, vmli);
        
        // Documentation (vic-ii.txt line 1184): "VC and VMLI are incremented after each g-access in display state."
        // CRITICAL DISTINCTION:
        // - On bad lines: VC and VMLI both increment during C-access (line 1342-1343)
        // - On non-bad lines: ONLY VMLI increments during G-access, VC stays at VCBASE
        //
        // Why? VC (Video Counter) points to the video matrix position and should only
        // advance on bad lines when new character codes are fetched. VMLI (Video Matrix
        // Line Index) is the column counter within the 40-character line buffer and must
        // increment on ALL lines to sequence through the 40 characters.
        //
        // CRITICAL BUG FIX: The pixel sequencer needs to read from the graphics_line position
        // that was JUST written (using the vmli value BEFORE increment). Store this value
        // so the pixel sequencer can use it.
        vicii->sequencer.current_vmli_for_display = vmli;
        
        if (!vicii->video_logic.is_bad_line) {
            // On non-bad lines: increment ONLY VMLI, not VC
            vicii->video_logic.vmli++;
        }
    }
    
    // STEP 5: Update border flip-flops to establish display window state
    // CRITICAL: This must happen BEFORE pixel sequencing so the sequencer sees the correct flip-flop state
    vicii_border_update_flip_flops_x(&vicii->border, &vicii->timing, vicii->registers.data[VICII_C1], vicii->config);

    // STEP 6: Perform unified pixel sequencing (8 pixels per cycle)
    // This uses the graphics data that was JUST loaded above AND the border flip-flop state updated above
    if (vicii->pixel.framebuffer && vicii->timing.raster_counter < vicii->pixel.framebuffer_height) {
        vicii_pixel_sequencer(vicii);
    }
    
    // STEP 7: Advance x_coordinate (primary counter) and update derived values
    // Note: vicii_timing_advance() now handles flushing and buffer clearing when wrapping to next line
    vicii_timing_advance(vicii);
    vicii_update_badline_condition(vicii);

    // Store pending access type for next cycle
    vicii->bus.pending_phi2_access_type = access_type;

    // STEP 10: Set up PHI2 memory access on the bus (C/P/S accesses only)
    // These will be serviced externally and read at the start of the next cycle
    switch (access_type) {
        case VIC_ACCESS_P:
            address = vicii->memory.vm_base | (0x3F8 + (uint16_t)access_param);
            break;
        case VIC_ACCESS_S: {
            vicii_sprite_unit_t* sprite = &vicii->sprites.sprites[access_param];

            // Sprites are 24 pixels wide, requiring exactly 3 bytes of data per line.
            // The VIC-II performs 3 S-accesses per sprite per raster line (mc_counter 0, 1, 2).
            // S-accesses only occur when the sprite sequencer calls this function,
            // which happens exactly 3 times per enabled sprite per raster line.
            // Therefor, there is no need to check for mc_counter overflow here.
            address = sprite->data_pointer * 64 + sprite->mc_counter;
            sprite->mc_counter++;
            break;
        }
        case VIC_ACCESS_C:
            // PHI2 access: Set up video matrix read (Color RAM will be read in-place during PHI1)
            // Data will arrive in the NEXT cycle and be stored at the current VMLI position
            // Use current VC BEFORE increment (increment happens after g-access)
            address = vicii->memory.vm_base | vicii->video_logic.vc;
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
// CYCLE TABLES
// ========================================================================================

// Cycle callback table - PAL timing (6569 : 63 cycles per line) (Documentation section 3.6.3)
static const vicii_cycle_entry_t vicii_cycle_table_6569[63] = {
    // 1=VIC-II PHI1, 2=VIC-II PHI2, C=CPU PHI2       12C 12C
    //                                                BAD NBD
    {vicii_cycle_sprite_p_access, 3},           // 1  3_x 3_x (PAL: sprite P-access only, line 0 ops postponed to next cycle)
    {vicii_cycle_sprite_s_1_pal, 3},            // 2  i_x i_x (PAL: executes postponed line 0 raster/IRQ operations)
    {vicii_cycle_sprite_p_access, 4},           // 3  4_x 4_x
    {vicii_cycle_sprite_s_access, 4},           // 4  i_x i_x
    {vicii_cycle_sprite_p_access, 5},           // 5  5_x 5_x
    {vicii_cycle_sprite_s_access, 5},           // 6  i_x i_x
    {vicii_cycle_sprite_p_access, 6},           // 7  6_x 6_x
    {vicii_cycle_sprite_s_access, 6},           // 8  i_x i_x
    {vicii_cycle_sprite_p_access, 7},           // 9  7_x 7_x
    {vicii_cycle_sprite_s_access, 7},           // 10 i_x i_x
    {vicii_cycle_refresh, -1},                  // 11 r_x r_x
    {vicii_cycle_refresh, -1},                  // 12 r_X r_x
    {vicii_cycle_refresh, -1},                  // 13 r_X r_x
    {vicii_cycle_refresh, -1},                  // 14 r_X r_x
    {vicii_cycle_vc_load_mcbase, -1},           // 15 rc_ r_x (+ MCBASE expansion check)
    {vicii_cycle_char_color_expansion_check, 0},// 16 gc_ g_x (+ expansion flip-flop check)
    {vicii_cycle_char_color_access, 1},         // 17 gc_ g_x
    {vicii_cycle_char_color_access, 2},         // 18 gc_ g_x
    {vicii_cycle_char_color_access, 3},         // 19 gc_ g_x
    {vicii_cycle_char_color_access, 4},         // 20 gc_ g_x
    {vicii_cycle_char_color_access, 5},         // 21 gc_ g_x
    {vicii_cycle_char_color_access, 6},         // 22 gc_ g_x
    {vicii_cycle_char_color_access, 7},         // 23 gc_ g_x
    {vicii_cycle_char_color_access, 8},         // 24 gc_ g_x
    {vicii_cycle_char_color_access, 9},         // 25 gc_ g_x
    {vicii_cycle_char_color_access, 10},        // 26 gc_ g_x
    {vicii_cycle_char_color_access, 11},        // 27 gc_ g_x
    {vicii_cycle_char_color_access, 12},        // 28 gc_ g_x
    {vicii_cycle_char_color_access, 13},        // 29 gc_ g_x
    {vicii_cycle_char_color_access, 14},        // 30 gc_ g_x
    {vicii_cycle_char_color_access, 15},        // 31 gc_ g_x
    {vicii_cycle_char_color_access, 16},        // 32 gc_ g_x
    {vicii_cycle_char_color_access, 17},        // 33 gc_ g_x
    {vicii_cycle_char_color_access, 18},        // 34 gc_ g_x
    {vicii_cycle_char_color_access, 19},        // 35 gc_ g_x
    {vicii_cycle_char_color_access, 20},        // 36 gc_ g_x
    {vicii_cycle_char_color_access, 21},        // 37 gc_ g_x
    {vicii_cycle_char_color_access, 22},        // 38 gc_ g_x
    {vicii_cycle_char_color_access, 23},        // 39 gc_ g_x
    {vicii_cycle_char_color_access, 24},        // 40 gc_ g_x
    {vicii_cycle_char_color_access, 25},        // 41 gc_ g_x
    {vicii_cycle_char_color_access, 26},        // 42 gc_ g_x
    {vicii_cycle_char_color_access, 27},        // 43 gc_ g_x
    {vicii_cycle_char_color_access, 28},        // 44 gc_ g_x
    {vicii_cycle_char_color_access, 29},        // 45 gc_ g_x
    {vicii_cycle_char_color_access, 30},        // 46 gc_ g_x
    {vicii_cycle_char_color_access, 31},        // 47 gc_ g_x
    {vicii_cycle_char_color_access, 32},        // 48 gc_ g_x
    {vicii_cycle_char_color_access, 33},        // 49 gc_ g_x
    {vicii_cycle_char_color_access, 34},        // 50 gc_ g_x
    {vicii_cycle_char_color_access, 35},        // 51 gc_ g_x
    {vicii_cycle_char_color_access, 36},        // 52 gc_ g_x
    {vicii_cycle_char_color_access, 37},        // 53 gc_ g_x
    {vicii_cycle_char_color_access, 38},        // 54 gc_ g_x
    {vicii_cycle_char_color_y_match, 39},       // 55 g_x g_x (+ sprite Y match)
    {vicii_cycle_idle_y_match, -1},             // 56 i_x i_x (+ sprite Y match)
    {vicii_cycle_idle, -1},                     // 57 i_x i_x
    {vicii_cycle_sprite_p_rc_mc_load, 0},       // 58 0_x 0_x : Sprite 0 P-access + RC/VCBASE + MC load (Rules 5 & 4)
    {vicii_cycle_sprite_s_access, 0},           // 59 i_x i_x : Sprite 0 S-access
    {vicii_cycle_sprite_p_access, 1},           // 60 1_x 1_x : Sprite 1 P-access
    {vicii_cycle_sprite_s_access, 1},           // 61 i_x i_x : Sprite 1 S-access
    {vicii_cycle_sprite_p_access, 2},           // 62 2_x 2_x : Sprite 2 P-access
    {vicii_cycle_sprite_s_border_check, 2}      // 63 i_x i_x : Sprite 2 S-access + border check
};

// Cycle callback table - NTSC timing (6567R56A : 64 cycles per line)
static const vicii_cycle_entry_t vicii_cycle_table_6567R56A[64] = {
    // 1=VIC-II PHI1, 2=VIC-II PHI2, C=CPU PHI2       12C 12C
    //                                                BAD NBD
    {vicii_cycle_sprite_p_0_ntsc, 3},           // 1  3_x 3_x (NTSC: executes line 0 raster/IRQ immediately)
    {vicii_cycle_sprite_s_access, 3},           // 2  i_x i_x
    {vicii_cycle_sprite_p_access, 4},           // 3  4_x 4_x
    {vicii_cycle_sprite_s_access, 4},           // 4  i_x i_x
    {vicii_cycle_sprite_p_access, 5},           // 5  5_x 5_x
    {vicii_cycle_sprite_s_access, 5},           // 6  i_x i_x
    {vicii_cycle_sprite_p_access, 6},           // 7  6_x 6_x
    {vicii_cycle_sprite_s_access, 6},           // 8  i_x i_x
    {vicii_cycle_sprite_p_access, 7},           // 9  7_x 7_x
    {vicii_cycle_sprite_s_access, 7},           // 10 i_x i_x
    {vicii_cycle_refresh, -1},                  // 11 r_x r_x
    {vicii_cycle_refresh, -1},                  // 12 r_X r_x
    {vicii_cycle_refresh, -1},                  // 13 r_X r_x
    {vicii_cycle_refresh, -1},                  // 14 r_X r_x
    {vicii_cycle_vc_load_mcbase, -1},           // 15 rc_ r_x (+ MCBASE expansion check)
    {vicii_cycle_char_color_expansion_check, 0},// 16 gc_ g_x (+ expansion flip-flop check)
    {vicii_cycle_char_color_access, 1},         // 17 gc_ g_x
    {vicii_cycle_char_color_access, 2},         // 18 gc_ g_x
    {vicii_cycle_char_color_access, 3},         // 19 gc_ g_x
    {vicii_cycle_char_color_access, 4},         // 20 gc_ g_x
    {vicii_cycle_char_color_access, 5},         // 21 gc_ g_x
    {vicii_cycle_char_color_access, 6},         // 22 gc_ g_x
    {vicii_cycle_char_color_access, 7},         // 23 gc_ g_x
    {vicii_cycle_char_color_access, 8},         // 24 gc_ g_x
    {vicii_cycle_char_color_access, 9},         // 25 gc_ g_x
    {vicii_cycle_char_color_access, 10},        // 26 gc_ g_x
    {vicii_cycle_char_color_access, 11},        // 27 gc_ g_x
    {vicii_cycle_char_color_access, 12},        // 28 gc_ g_x
    {vicii_cycle_char_color_access, 13},        // 29 gc_ g_x
    {vicii_cycle_char_color_access, 14},        // 30 gc_ g_x
    {vicii_cycle_char_color_access, 15},        // 31 gc_ g_x
    {vicii_cycle_char_color_access, 16},        // 32 gc_ g_x
    {vicii_cycle_char_color_access, 17},        // 33 gc_ g_x
    {vicii_cycle_char_color_access, 18},        // 34 gc_ g_x
    {vicii_cycle_char_color_access, 19},        // 35 gc_ g_x
    {vicii_cycle_char_color_access, 20},        // 36 gc_ g_x
    {vicii_cycle_char_color_access, 21},        // 37 gc_ g_x
    {vicii_cycle_char_color_access, 22},        // 38 gc_ g_x
    {vicii_cycle_char_color_access, 23},        // 39 gc_ g_x
    {vicii_cycle_char_color_access, 24},        // 40 gc_ g_x
    {vicii_cycle_char_color_access, 25},        // 41 gc_ g_x
    {vicii_cycle_char_color_access, 26},        // 42 gc_ g_x
    {vicii_cycle_char_color_access, 27},        // 43 gc_ g_x
    {vicii_cycle_char_color_access, 28},        // 44 gc_ g_x
    {vicii_cycle_char_color_access, 29},        // 45 gc_ g_x
    {vicii_cycle_char_color_access, 30},        // 46 gc_ g_x
    {vicii_cycle_char_color_access, 31},        // 47 gc_ g_x
    {vicii_cycle_char_color_access, 32},        // 48 gc_ g_x
    {vicii_cycle_char_color_access, 33},        // 49 gc_ g_x
    {vicii_cycle_char_color_access, 34},        // 50 gc_ g_x
    {vicii_cycle_char_color_access, 35},        // 51 gc_ g_x
    {vicii_cycle_char_color_access, 36},        // 52 gc_ g_x
    {vicii_cycle_char_color_access, 37},        // 53 gc_ g_x
    {vicii_cycle_char_color_access, 38},        // 54 gc_ g_x
    {vicii_cycle_char_color_y_match, 39},       // 55 g_x g_x (+ sprite Y match)
    {vicii_cycle_idle_y_match, -1},             // 56 i_x i_x (+ sprite Y match)
    {vicii_cycle_idle, -1},                     // 57 i_x i_x
    {vicii_cycle_idle, -1},                     // 58 i_x i_x
    {vicii_cycle_sprite_p_rc_mc_load, 0},       // 59 0_x 0_x : Sprite 0 P-access + RC/VCBASE + MC load (Rules 5 & 4)
    {vicii_cycle_sprite_s_access, 0},           // 60 i_x i_x : Sprite 0 S-access
    {vicii_cycle_sprite_p_access, 1},           // 61 1_x 1_x : Sprite 1 P-access
    {vicii_cycle_sprite_s_access, 1},           // 62 i_x i_x : Sprite 1 S-access
    {vicii_cycle_sprite_p_access, 2},           // 63 2_x 2_x : Sprite 2 P-access
    {vicii_cycle_sprite_s_border_check, 2}      // 64 i_x i_x : Sprite 2 S-access + border check
};

// Cycle callback table - NTSC timing (6567R8 : 65 cycles per line)
static const vicii_cycle_entry_t vicii_cycle_table_6567R8[65] = {
    // 1=VIC-II PHI1, 2=VIC-II PHI2, C=CPU PHI2       12C 12C
    //                                                BAD NBD
    {vicii_cycle_sprite_p_0_ntsc, 3},           // 1  3_x 3_x (NTSC: executes line 0 raster/IRQ immediately)
    {vicii_cycle_sprite_s_access, 3},           // 2  i_x i_x
    {vicii_cycle_sprite_p_access, 4},           // 3  4_x 4_x
    {vicii_cycle_sprite_s_access, 4},           // 4  i_x i_x
    {vicii_cycle_sprite_p_access, 5},           // 5  5_x 5_x
    {vicii_cycle_sprite_s_access, 5},           // 6  i_x i_x
    {vicii_cycle_sprite_p_access, 6},           // 7  6_x 6_x
    {vicii_cycle_sprite_s_access, 6},           // 8  i_x i_x
    {vicii_cycle_sprite_p_access, 7},           // 9  7_x 7_x
    {vicii_cycle_sprite_s_access, 7},           // 10 i_x i_x
    {vicii_cycle_refresh, -1},                  // 11 r_x r_x
    {vicii_cycle_refresh, -1},                  // 12 r_X r_x
    {vicii_cycle_refresh, -1},                  // 13 r_X r_x
    {vicii_cycle_refresh, -1},                  // 14 r_X r_x
    {vicii_cycle_vc_load_mcbase, -1},           // 15 rc_ r_x (+ MCBASE expansion check)
    {vicii_cycle_char_color_expansion_check, 0},// 16 gc_ g_x (+ expansion flip-flop check)
    {vicii_cycle_char_color_access, 1},         // 17 gc_ g_x
    {vicii_cycle_char_color_access, 2},         // 18 gc_ g_x
    {vicii_cycle_char_color_access, 3},         // 19 gc_ g_x
    {vicii_cycle_char_color_access, 4},         // 20 gc_ g_x
    {vicii_cycle_char_color_access, 5},         // 21 gc_ g_x
    {vicii_cycle_char_color_access, 6},         // 22 gc_ g_x
    {vicii_cycle_char_color_access, 7},         // 23 gc_ g_x
    {vicii_cycle_char_color_access, 8},         // 24 gc_ g_x
    {vicii_cycle_char_color_access, 9},         // 25 gc_ g_x
    {vicii_cycle_char_color_access, 10},        // 26 gc_ g_x
    {vicii_cycle_char_color_access, 11},        // 27 gc_ g_x
    {vicii_cycle_char_color_access, 12},        // 28 gc_ g_x
    {vicii_cycle_char_color_access, 13},        // 29 gc_ g_x
    {vicii_cycle_char_color_access, 14},        // 30 gc_ g_x
    {vicii_cycle_char_color_access, 15},        // 31 gc_ g_x
    {vicii_cycle_char_color_access, 16},        // 32 gc_ g_x
    {vicii_cycle_char_color_access, 17},        // 33 gc_ g_x
    {vicii_cycle_char_color_access, 18},        // 34 gc_ g_x
    {vicii_cycle_char_color_access, 19},        // 35 gc_ g_x
    {vicii_cycle_char_color_access, 20},        // 36 gc_ g_x
    {vicii_cycle_char_color_access, 21},        // 37 gc_ g_x
    {vicii_cycle_char_color_access, 22},        // 38 gc_ g_x
    {vicii_cycle_char_color_access, 23},        // 39 gc_ g_x
    {vicii_cycle_char_color_access, 24},        // 40 gc_ g_x
    {vicii_cycle_char_color_access, 25},        // 41 gc_ g_x
    {vicii_cycle_char_color_access, 26},        // 42 gc_ g_x
    {vicii_cycle_char_color_access, 27},        // 43 gc_ g_x
    {vicii_cycle_char_color_access, 28},        // 44 gc_ g_x
    {vicii_cycle_char_color_access, 29},        // 45 gc_ g_x
    {vicii_cycle_char_color_access, 30},        // 46 gc_ g_x
    {vicii_cycle_char_color_access, 31},        // 47 gc_ g_x
    {vicii_cycle_char_color_access, 32},        // 48 gc_ g_x
    {vicii_cycle_char_color_access, 33},        // 49 gc_ g_x
    {vicii_cycle_char_color_access, 34},        // 50 gc_ g_x
    {vicii_cycle_char_color_access, 35},        // 51 gc_ g_x
    {vicii_cycle_char_color_access, 36},        // 52 gc_ g_x
    {vicii_cycle_char_color_access, 37},        // 53 gc_ g_x
    {vicii_cycle_char_color_access, 38},        // 54 gc_ g_x
    {vicii_cycle_char_color_y_match, 39},       // 55 g_x g_x (+ sprite Y match)
    {vicii_cycle_idle_y_match, -1},             // 56 i_x i_x (+ sprite Y match)
    {vicii_cycle_idle, -1},                     // 57 i_x i_x
    {vicii_cycle_idle, -1},                     // 58 i_x i_x
    {vicii_cycle_idle, -1},                     // 59 i_x i_x
    {vicii_cycle_sprite_p_rc_mc_load, 0},       // 60 0_x 0_x : Sprite 0 P-access + RC/VCBASE + MC load (Rules 5 & 4)
    {vicii_cycle_sprite_s_access, 0},           // 61 i_x i_x : Sprite 0 S-access
    {vicii_cycle_sprite_p_access, 1},           // 62 1_x 1_x : Sprite 1 P-access
    {vicii_cycle_sprite_s_access, 1},           // 63 i_x i_x : Sprite 1 S-access
    {vicii_cycle_sprite_p_access, 2},           // 64 2_x 2_x : Sprite 2 P-access
    {vicii_cycle_sprite_s_border_check, 2}      // 65 i_x i_x : Sprite 2 S-access + border check
};

// ========================================================================================
// CHIP CONFIGURATION DEFINITIONS
// ========================================================================================

// MOS6567(R56A) NTSC VIC-II Configuration
static const vicii_chip_config_t MOS6567R56A_config = {
    .total_lines = 262,
    .visible_lines = 234,
    .cycles_per_line = 64,
    .visible_pixels_per_line = 411,
    .first_vblank_line = 13,
    .last_vblank_line = 40,
    .first_x_coord = 412, // ($19c)
    .first_visible_x_coord = 488, // ($1e8)
    .last_visible_x_coord = 388, // ($184)
    
    .framebuffer_start_x = 0,
    .framebuffer_end_x = 520,  // Allow full scanline width to accommodate pipeline delay wrap-around
    
    .chip_name = "MOS6567(R56A) NTSC"
};

// MOS6567(R8) NTSC VIC-II Configuration
static const vicii_chip_config_t MOS6567R8_config = {
    .total_lines = 262,
    .visible_lines = 235,
    .cycles_per_line = 65,
    .visible_pixels_per_line = 411,
    .first_vblank_line = 13,
    .last_vblank_line = 40,
    .first_x_coord = 412, // ($19c)
    .first_visible_x_coord = 489, // ($1e9)
    .last_visible_x_coord = 396, // ($18c)
    
    .framebuffer_start_x = 0,
    .framebuffer_end_x = 520,  // Allow full scanline width to accommodate pipeline delay wrap-around
    
    .chip_name = "MOS6567(R8) NTSC"
};

// MOS6569 PAL VIC-II Configuration
static const vicii_chip_config_t MOS6569_config = {
    .total_lines = 312,
    .visible_lines = 284,
    .cycles_per_line = 63,
    .visible_pixels_per_line = 403,
    .first_vblank_line = 300,
    .last_vblank_line = 15,
    .first_x_coord = 404, // ($194)
    .first_visible_x_coord = 480, // ($1e0)
    .last_visible_x_coord = 380, // ($17c)
    
    .framebuffer_start_x = 0,
    .framebuffer_end_x = 504,  // Allow full scanline width to accommodate pipeline delay wrap-around
    
    .chip_name = "MOS6569 PAL"
};

// ========================================================================================
// INITIALIZATION
// ========================================================================================

static inline void vicii_initialize(vicii_t* vicii) {
    // Zero all units
    memset(&vicii->registers, 0, sizeof(vicii_registers_unit_t));
    memset(&vicii->timing, 0, sizeof(vicii_timing_unit_t));
    memset(&vicii->video_logic, 0, sizeof(vicii_video_logic_unit_t));
    memset(&vicii->video_data, 0, sizeof(vicii_video_data_unit_t));
    memset(&vicii->sequencer, 0, sizeof(vicii_sequencer_unit_t));
    memset(&vicii->border, 0, sizeof(vicii_border_unit_t));
    memset(&vicii->memory, 0, sizeof(vicii_memory_unit_t));
    memset(&vicii->sprites, 0, sizeof(vicii_sprites_unit_t));
    memset(&vicii->pixel, 0, sizeof(vicii_pixel_unit_t));
    memset(&vicii->bus, 0, sizeof(vicii_bus_unit_t));
    
    // Set default register values - Enable DEN to match real hardware behavior
    // The VIC-II starts with display enabled, allowing immediate character data display
    vicii->registers.data[VICII_C1] = VICII_C1_RST8 | VICII_C1_DEN | VICII_C1_RSEL |
                            (VICII_C1_YSCROLL & 3); // DEN=1, YSCROLL=3, RSEL=1, RST8=1
    vicii->registers.data[VICII_MXE] = 0;  // All sprites disabled
    vicii->registers.data[VICII_C2] = VICII_C2_CSEL; // 8: XSCROLL:0, no MultiColorMode, 40-column display, no RESET
    vicii->registers.data[VICII_MP] = VICII_MP_CB12 | VICII_MP_VM10; // 0x14: "address of Character Dot-Data area to 4096 ($1000)"
    vicii->registers.data[VICII_IR] = VICII_IR_UNUSED; // See BusWrite; Always set the unused bits high
    
    // Set default colors
    vicii->registers.data[VICII_EC] = VICII_COLOR_LIGHT_BLUE; // 14: Border Color
    vicii->registers.data[VICII_B0C] = VICII_COLOR_BLUE; // 6: Background Color 0
    vicii->registers.data[VICII_B1C] = VICII_COLOR_WHITE; // 1: Background Color 1
    vicii->registers.data[VICII_B2C] = VICII_COLOR_RED; // 2: Background Color 2
    vicii->registers.data[VICII_B3C] = VICII_COLOR_CYAN; // 3: Background Color 3
    vicii->registers.data[VICII_MM0] = VICII_COLOR_PURPLE; // 4: Sprite Multicolor 0
    vicii->registers.data[VICII_MM1] = VICII_COLOR_BLACK; // 0: Sprite Multicolor 1
    vicii->registers.data[VICII_M0C] = VICII_COLOR_WHITE; // 1: Sprite Color 0
    vicii->registers.data[VICII_M1C] = VICII_COLOR_RED; // 2: Sprite Color 1
    vicii->registers.data[VICII_M2C] = VICII_COLOR_CYAN; // 3: Sprite Color 2
    vicii->registers.data[VICII_M3C] = VICII_COLOR_PURPLE; // 4: Sprite Color 3
    vicii->registers.data[VICII_M4C] = VICII_COLOR_GREEN; // 5: Sprite Color 4
    vicii->registers.data[VICII_M5C] = VICII_COLOR_BLUE; // 6: Sprite Color 5
    vicii->registers.data[VICII_M6C] = VICII_COLOR_YELLOW; // 7: Sprite Color 6
    vicii->registers.data[VICII_M7C] = VICII_COLOR_MEDIUM_GREY; // 12: Sprite Color 7
    
    // Initialize color priorities
    vicii->sequencer.colors[0].priority = VICII_PRIORITY_BACKGROUND; // "00" / "0" Use in both MC modes
    vicii->sequencer.colors[1].priority = VICII_PRIORITY_BACKGROUND; // "01" Used in EmitMCPixel()
    vicii->sequencer.colors[2].priority = VICII_PRIORITY_FOREGROUND; // "10"
    vicii->sequencer.colors[3].priority = VICII_PRIORITY_FOREGROUND; // "11"
    vicii->sequencer.colors[4].priority = VICII_PRIORITY_FOREGROUND; // "1" Used in EmitPixel()
    
    // Initialize sequencer
    vicii->sequencer.last_mode = 0xFF;
    vicii->sequencer.shift_reg = 0;
    vicii->sequencer.xscroll_counter = 0;
    
    // Update units based on register values
    vicii_sequencer_update_mode(&vicii->sequencer, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
    vicii_border_update_limits(&vicii->border, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
    
    // Initialize border pixel from register value (EC was set to LIGHT_BLUE at line 1815)
    vicii->border.border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border.border_pixel.color = static_cast<vicii_color_t>(vicii->registers.data[VICII_EC]);
    // Initialize border flip-flops (Documentation section 3.9)
    vicii->border.main_border_flip_flop = true;      // Start with border on
    vicii->border.vertical_border_flip_flop = true;  // Start with vertical border on
    vicii_memory_update_mapping(&vicii->memory, vicii->registers.data[VICII_MP]);
    
    // Initialize refresh counter (Documentation section 3.13)
    vicii->video_logic.refresh_counter = 0xFF;
    
    // Initialize bad line detection
    // Since DEN is enabled at startup (line 1716), we must set was_den_set_during_raster_30
    // to true so that bad lines can occur immediately. Without this, the first frame would
    // have no bad lines and therefore no character data display.
    vicii->video_logic.was_den_set_during_raster_30 = true;
    vicii->video_logic.ba_low_for_bad_line = false;
    
    // Pixel buffers will be allocated in timing initialization
}

static inline void vicii_initialize_timing(vicii_t* vicii, const vicii_chip_config_t* config) {
    // Select cycle table based on timing characteristics
    if (config->cycles_per_line == 63) { // PAL
        vicii->timing.cycle_table = vicii_cycle_table_6569;
    } else if (config->cycles_per_line == 64) { // NTSC
        vicii->timing.cycle_table = vicii_cycle_table_6567R56A;
    } else if (config->cycles_per_line == 65) { // NTSC
        vicii->timing.cycle_table = vicii_cycle_table_6567R8;
    } else {
        // Default to PAL if unknown
        vicii->timing.cycle_table = vicii_cycle_table_6569;
    }
    
    // Allocate pixel buffers based on config
    if (config->visible_pixels_per_line > 0) {
        // Free any existing buffers
        free(vicii->pixel.pixel_line_priority);
        free(vicii->pixel.pixel_line_color);
        
                // Allocate single line buffers
                vicii->pixel.pixel_line_priority = static_cast<vicii_priority_t*>(malloc(config->visible_pixels_per_line * sizeof(vicii_priority_t)));
                vicii->pixel.pixel_line_color = static_cast<uint8_t*>(malloc(config->visible_pixels_per_line * sizeof(uint8_t)));
                
                // Initialize buffer with current border color
                const uint8_t border_color = vicii->registers.data[VICII_EC] & 0x0F;
                memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, config->visible_pixels_per_line);
                memset(vicii->pixel.pixel_line_color, border_color, config->visible_pixels_per_line);
    }
    
    vicii_set_x_cycle(vicii, 0);
    vicii->timing.raster_counter = 0;
    
    // Initialize border flip-flops to show border initially
    vicii->border.main_border_flip_flop = true;
    vicii->border.vertical_border_flip_flop = true;
}

// ========================================================================================
// PUBLIC API FUNCTIONS
// ========================================================================================

vicii_t* vicii_system_create(chip_descriptor_t* desc, const vicii_chip_config_t* config, void (*bank_change)(void*, uint8_t)) {
    vicii_t* vicii = (vicii_t*)calloc(1, sizeof(vicii_t));
    if (!vicii) return NULL;
    
    vicii->desc = desc;
    vicii->config = config;
    vicii->bus.bank_change = bank_change;
    vicii_initialize(vicii);
    vicii_initialize_timing(vicii, config);
    
    return vicii;
}

void vicii_system_destroy(void* chip) {
    vicii_t* vicii = (vicii_t*)chip;
    if (vicii) {
        // Free buffers
        free(vicii->pixel.pixel_line_priority);
        free(vicii->pixel.pixel_line_color);
        free(vicii);
    }
}

void vicii_bus_attach(void* chip, void* bus) {
    ((vicii_t*)chip)->bus.bus = bus;
}

// Configuration helper function
const vicii_chip_config_t* vicii_get_default_config(bool is_pal) {
    return is_pal ? &MOS6569_config : &MOS6567R8_config;
}

void vicii_set_framebuffer(vicii_t* vicii, uint32_t* framebuffer, int width, int height) {
    vicii_pixel_set_framebuffer(&vicii->pixel, framebuffer, width, height);
    // Border color should come from register, not hardcoded
    vicii->border.border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border.border_pixel.color = static_cast<vicii_color_t>(vicii->registers.data[VICII_EC]);
        if (vicii->pixel.pixel_line_color && vicii->config->visible_pixels_per_line > 0) {
            // Initialize buffer with current border color
            const uint8_t border_color = vicii->registers.data[VICII_EC] & 0x0F;
            memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, vicii->config->visible_pixels_per_line);
            memset(vicii->pixel.pixel_line_color, border_color, vicii->config->visible_pixels_per_line);
        }
}
