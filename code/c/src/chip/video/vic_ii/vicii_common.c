#include "vicii_common.h"
#include "../../../systems/c64/c64_bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ========================================================================================
// CONSTANTS AND STATIC DATA
// ========================================================================================

// C64 color palette - RGBA format
static const uint32_t c64_palette[16] = {
    0xFF000000, 0xFFFFFFFF, 0xFF2B3768, 0xFFB2A470,
    0xFF863D6F, 0xFF438D58, 0xFF792835, 0xFF6FC7B8,
    0xFF254F6F, 0xFF003943, 0xFF59679A, 0xFF444444,
    0xFF6C6C6C, 0xFF84D29A, 0xFFB55E6C, 0xFF959595
};

uint32_t* vicii_common_get_default_palette(void) {
    return (uint32_t*)c64_palette;
}

// ========================================================================================
// INLINE UTILITY FUNCTIONS
// ========================================================================================

// Bus control helpers
static inline void vic_bus_control_aec_high(vicii_common_t* vicii) {
    ((c64_bus_t*)vicii->bus.bus)->control_lines |= AEC_LINE;
}

static inline void vic_bus_control_aec_low(vicii_common_t* vicii) {
    ((c64_bus_t*)vicii->bus.bus)->control_lines &= ~AEC_LINE;
}

static inline void vic_bus_control_ba_high(vicii_common_t* vicii) {
    ((c64_bus_t*)vicii->bus.bus)->control_lines |= BA_LINE;
}

static inline void vic_bus_control_ba_low(vicii_common_t* vicii) {
    ((c64_bus_t*)vicii->bus.bus)->control_lines &= ~BA_LINE;
}

// ========================================================================================
// BORDER LOGIC
// ========================================================================================

// Pixel emission helper
static inline void vic_pixel_emit_single(vic_pixel_unit_t* pixel, const vicii_pixel_t* pixel_data) {
    if (pixel->pixel_line_index < pixel->visible_pixels_per_line) {
        uint16_t idx = pixel->pixel_line_index++;

        pixel->pixel_line_priority[idx] = pixel_data->priority;
        pixel->pixel_line_color[idx] = pixel_data->color;
    }
}

static inline void vic_border_emit_pixels(vicii_common_t* vicii) {
    vic_pixel_emit_single(&vicii->pixel, &vicii->border.border_pixel);
}

static inline void vic_border_update_limits(vic_border_unit_t* border, uint8_t c1_reg, uint8_t c2_reg) {
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
// PIXEL SEQUENCER AND GRAPHICS - X-COORDINATE DRIVEN
// ========================================================================================

// X-coordinate driven pixel emission for precise positioning
static inline void vic_pixel_emit_at_x(vicii_common_t* vicii, const vicii_pixel_t* pixel_data, uint16_t x_coord) {
    // Check if x_coord is within framebuffer bounds
    if (x_coord < vicii->pixel.framebuffer_start_x || x_coord >= vicii->pixel.framebuffer_end_x) return;
    
    // Calculate pixel position in line buffer
    uint16_t pixel_x = x_coord - vicii->pixel.framebuffer_start_x;
    if (pixel_x >= vicii->pixel.visible_pixels_per_line) return;
    
    // Store pixel data
    vicii->pixel.pixel_line_priority[pixel_x] = pixel_data->priority;
    vicii->pixel.pixel_line_color[pixel_x] = pixel_data->color;
}

// Graphics sequencer (called from memory access)
void vic_graphics_sequencer(vicii_common_t* vicii, uint8_t graphics_data) {
    // Use x_coordinate for pixel-perfect positioning
    uint16_t x_coord = vicii->timing.display_x_coordinate;
    
    // Check if we're in the visible display area
    if (x_coord < vicii->pixel.display_start_x || x_coord >= vicii->pixel.display_end_x) return;
    
    vic_sequencer_unit_t* seq = &vicii->sequencer;
    
    // Calculate pixel position within display
    uint16_t display_pixel_x = x_coord - vicii->pixel.display_start_x;
    
    // Initialize sequencer on line start only
    if (display_pixel_x == 0) {
        seq->xscroll_counter = vicii->registers.data[VICII_C2] & VICII_C2_XSCROLL;
        seq->char_index = 0;
        seq->pixel_in_char = 0;
    }

    // Reset shift register on mode change (but not x-scroll)
    if (seq->graphics_mode != seq->last_mode) {
        seq->last_mode = seq->graphics_mode;
        seq->shift_reg = 0;
    }
    
    // XSCROLL handling - delay pixel output by XSCROLL pixels
    if (seq->xscroll_counter > 0) {
        seq->xscroll_counter--;
        vic_pixel_emit_at_x(vicii, &seq->colors[0], x_coord);
        return;
    }
    
    // Load new graphics data every 8 pixels (one character width)
    if (seq->pixel_in_char == 0) {
        seq->shift_reg = graphics_data;
        seq->char_index = (uint8_t)(display_pixel_x >> 3); // Character position
    }
    
    // Extract pixel data based on mode
    uint8_t color_index = 0;
    uint8_t pixel_data = 0;
    vicii_pixel_t pixel;
    
    switch (seq->graphics_mode) {
        case VICII_GM_STANDARD_TEXT:
            pixel_data = (seq->shift_reg >> 7) & 1;
            color_index = pixel_data ? 4 : 0;
            seq->shift_reg <<= 1;
            break;
            
        case VICII_GM_MULTICOLOR_TEXT:
            if (vicii->video_data.video_color_line[seq->char_index] & 0x08) {
                // Multicolor character - 2 bits per pixel
                pixel_data = (seq->shift_reg >> 6) & 3;
                color_index = pixel_data;
                seq->shift_reg <<= 2;
                seq->pixel_in_char = (seq->pixel_in_char + 2) & 7;
            } else {
                // Standard character in multicolor mode
                pixel_data = (seq->shift_reg >> 7) & 1;
                color_index = pixel_data ? 4 : 0;
                seq->shift_reg <<= 1;
                seq->pixel_in_char = (seq->pixel_in_char + 1) & 7;
            }
            return; // Skip increment below
            
        case VICII_GM_STANDARD_BITMAP:
            pixel_data = (seq->shift_reg >> 7) & 1;
            if (pixel_data) {
                color_index = (vicii->video_data.video_matrix_line[seq->char_index] >> 4) & 0x0F;
            } else {
                color_index = vicii->video_data.video_matrix_line[seq->char_index] & 0x0F;
            }
            seq->shift_reg <<= 1;
            break;
            
        case VICII_GM_MULTICOLOR_BITMAP:
            pixel_data = (seq->shift_reg >> 6) & 3;
            switch (pixel_data) {
                case 0: color_index = vicii->registers.data[VICII_B0C]; break;
                case 1: color_index = (vicii->video_data.video_matrix_line[seq->char_index] >> 4) & 0x0F; break;
                case 2: color_index = vicii->video_data.video_matrix_line[seq->char_index] & 0x0F; break;
                case 3: color_index = vicii->video_data.video_color_line[seq->char_index] & 0x0F; break;
            }
            seq->shift_reg <<= 2;
            seq->pixel_in_char = (seq->pixel_in_char + 2) & 7;
            return; // Skip increment below
            
        case VICII_GM_ECM_TEXT:
            pixel_data = (seq->shift_reg >> 7) & 1;
            if (pixel_data) {
                color_index = vicii->video_data.video_color_line[seq->char_index] & 0x0F;
            } else {
                uint8_t bg_select = (vicii->video_data.video_matrix_line[seq->char_index] >> 6) & 3;
                color_index = vicii->registers.data[VICII_B0C + bg_select];
            }
            seq->shift_reg <<= 1;
            break;
            
        default:
            color_index = 0;
            break;
    }
    
    // Increment pixel position within character
    seq->pixel_in_char = (seq->pixel_in_char + 1) & 7;
    
    pixel.color = color_index;
    pixel.priority = (pixel_data == 0) ? VICII_PRIORITY_BACKGROUND : VICII_PRIORITY_FOREGROUND;
    vic_pixel_emit_at_x(vicii, &pixel, x_coord);
}

// ========================================================================================
// MEMORY ACCESS
// ========================================================================================

// Update memory mapping (Documentation section 2.4.2)
static inline void vic_memory_update_mapping(vic_memory_unit_t* memory, uint8_t mp_reg) {
    // Update addresses with bit operations
    // "VM10-VM13 (register $d018) that specify one of four 1KB blocks within the 16KB address space"
    memory->vm_base = ((uint16_t)mp_reg & 0xF0) << 6;  // VM10-VM13 bits * 0x400 -> << 6
    // "CB11-CB13 (register $d018) that specify one of eight 2KB blocks within the 16KB address space"
    memory->cb_base = ((uint16_t)mp_reg & 0x0E) << 10; // CB11-CB13 bits * 0x800 -> << 10
}

uint8_t vic_memory_read(vicii_common_t* vicii, uint16_t address) {
    if (!vicii->bus.bus) return 0xFF;
    
    // ULTRA-OPTIMIZED VIC-II MEMORY READ - Better performance than CPU version
    // VIC-II uses pre-selected active array (raw ACIDs), can only read, never write
    // Bank base offset applied here to keep operations in most appropriate place
    
    c64_bus_t* c64_bus = (c64_bus_t*)vicii->bus.bus;

    // Apply bank base offset to address
    uint16_t final_address = vicii->memory.bank_base | address;
    
    // Extract 4KB bank from address (0-15 for VIC-II's 64KB addressable space)
    uint8_t vic_bank = final_address >> 12;
    
    // Get raw ACID directly from pre-selected active array (no mode indexing)
    uint8_t read_acid = c64_bus->vic_ii_acid_per_bank[vic_bank];
    
    // Direct callback with final address including bank offset
    return c64_bus->read_callbacks[read_acid].read(
        c64_bus->read_callbacks[read_acid].context, final_address);
}

void vic_memory_access(vicii_common_t* vicii, uint8_t access_type, uint8_t access_param) {
    if (!vicii->bus.bus) return;
    
    uint16_t address;
    uint8_t data = 0;
    c64_bus_t* bus = (c64_bus_t*)vicii->bus.bus;
    
    switch (access_type) {
        case VIC_ACCESS_P:
            address = vicii->memory.vm_base + 0x3F8 + access_param;
            data = vic_memory_read(vicii, address);
            {
                vic_sprite_unit_t* sprite = &vicii->sprites.sprites[access_param];
                sprite->data_pointer = data;
            }
            break;
            
        case VIC_ACCESS_S:
            {
                vic_sprite_unit_t* sprite = &vicii->sprites.sprites[access_param];
                if (sprite->mc_counter < 3) {
                    address = sprite->data_pointer * 64 + sprite->mc_counter;
                    data = vic_memory_read(vicii, address);
                    sprite->data_buffer[sprite->mc_counter] = data;
                    sprite->mc_counter++;
                } else {
                    // "Whatever appears on the VIC-II internal bus during the fetch cycles
                    // is displayed. That is both loads and stores to the VIC-II, or $ff if
                    // no access occurs."
                    data = bus->data; // Use whatever is on the bus (sprite idle fetch)
                }
            }
            break;
            
        case VIC_ACCESS_C:
            // Color RAM access (pins D8-D11)
            // Note, that mos2114_read masks the address to 0x03FF
            // so for that there's no need to use base 0xD800, but
            // since the same adress is used for both C and G accesses,
            // we still calculate the absolute address
            // TODO : Should this incorporate vic_bank_base too? 
            address = 0xD800 + vicii->video_logic.vc;
            data = bus->read_callbacks[ACID_COLORRAM_D8].read(
                bus->read_callbacks[ACID_COLORRAM_D8].context, address);
            vicii->video_data.video_color_line[vicii->video_logic.vmli] = data & 0x0F;
            
            // Video matrix access  
            address = vicii->memory.vm_base + vicii->video_logic.vc;
            data = vic_memory_read(vicii, address);
            vicii->video_data.video_matrix_line[vicii->video_logic.vmli] = data;
            bus->data = data; // Set bus data for next access
            
            // Increment VC and VMLI after c-access in display state
            if (vicii->video_logic.display_state) {
                vicii->video_logic.vc++;
                vicii->video_logic.vmli++;
            }
            // Fall through to g-access
            
        case VIC_ACCESS_G:
            {
                // Get character code from the video matrix line (real chip behavior)
                uint8_t char_code = vicii->video_data.video_matrix_line[vicii->video_logic.vmli];
                
                if (vicii->video_logic.display_state) {
                    if (vicii->registers.data[VICII_C1] & VICII_C1_BMM) {
                        // Bitmap mode
                        address = vicii->memory.cb_base | 
                                ((vicii->video_logic.vc & 0x3FF) << 3) | 
                                (vicii->video_logic.rc & 0x07);
                    } else {
                        // Text mode
                        // "When changing from RAM to (char)ROM fetches, the LSB of the fetch address
                        // is latched using the mode from the previous cycle, and the upper bits come
                        // from the current mode. This glitch happens on 6569, but not on 8565."
                        address = vicii->memory.cb_base | 
                                (char_code << 3) | 
                                (vicii->video_logic.rc & 0x07);
                    }
                } else {
                    // Idle state
                    address = (vicii->registers.data[VICII_C1] & VICII_C1_ECM) ? 0x39ff : 0x3fff;
                }
                
                uint8_t graphics_data = vic_memory_read(vicii, address);
                vic_graphics_sequencer(vicii, graphics_data);
                data = graphics_data;
            }
            break;
            
        case VIC_ACCESS_REFRESH:
            if (vicii->enable_hardware_accurate_reads) {
                address = vicii->memory.vm_base | 0x3F00 | vicii->video_logic.refresh_counter;
                data = vic_memory_read(vicii, address);
            } else {
                data = bus->data; // Use floating bus data
            }
            vicii->video_logic.refresh_counter--;
            break;
            
        default: // VIC_ACCESS_IDLE
            if (vicii->enable_hardware_accurate_reads) {
                data = vic_memory_read(vicii, 0x3FFF);
            } else {
                data = bus->data; // Use floating bus data
            }
            break;
    }
    
    bus->data = data;
}

// ========================================================================================
// SPRITE HANDLING
// ========================================================================================

static inline void vic_sprite_emit_pixels(vicii_common_t* vicii, int sprite_index) {
    vic_sprite_unit_t* sprite = &vicii->sprites.sprites[sprite_index];
    
    if (!sprite->enabled || !sprite->display_state) return;
    
    uint16_t sprite_x = (vicii->registers.data[VICII_M0X + sprite_index * 2]) |
                       ((vicii->registers.data[VICII_MX8] & (1 << sprite_index)) ? 0x100 : 0);
    
    uint16_t current_x = vicii->timing.display_x_coordinate;
    
    if (current_x < sprite_x || current_x >= (sprite_x + 24)) return;
    
    uint8_t sprite_pixel_x = (uint8_t)(current_x - sprite_x);
    
    if (vicii->registers.data[VICII_MXXE] & (1 << sprite_index)) {
        sprite_pixel_x >>= 1;
    }
    
    uint32_t pixel_mask = 0x800000 >> sprite_pixel_x;
    bool sprite_pixel = (sprite->shift_reg & pixel_mask) != 0;
    
    if (!sprite_pixel) return;
    
    // Calculate pixel position in line buffer
    uint16_t pixel_x = current_x - vicii->pixel.framebuffer_start_x;
    if (pixel_x >= vicii->pixel.visible_pixels_per_line) return;
    
    vicii_priority_t current_priority = vicii->pixel.pixel_line_priority[pixel_x];
    
    // Collision detection
    if (current_priority == VICII_PRIORITY_SPRITE_IN_FRONT || 
        current_priority == VICII_PRIORITY_SPRITE_BEHIND) {
        vicii->registers.data[VICII_MXM_2] |= (1 << sprite_index);
    }
    
    if (current_priority == VICII_PRIORITY_FOREGROUND) {
        vicii->registers.data[VICII_MXD_2] |= (1 << sprite_index);
    }
    
    // Color determination
    uint8_t sprite_color;
    bool is_multicolor = (vicii->registers.data[VICII_MXMC] & (1 << sprite_index)) != 0;
    
    if (is_multicolor) {
        uint8_t bit_pair = (sprite->shift_reg >> (22 - sprite_pixel_x)) & 3;
        switch (bit_pair) {
            default: //  avoids a compiler warning
            case 0: return;
            case 1: sprite_color = vicii->registers.data[VICII_MM0]; break;
            case 2: sprite_color = vicii->registers.data[VICII_M0C + sprite_index]; break;
            case 3: sprite_color = vicii->registers.data[VICII_MM1]; break;
        }
    } else {
        sprite_color = vicii->registers.data[VICII_M0C + sprite_index];
    }
    
    // Priority check
    bool sprite_wins = false;
    if (sprite->priority == VICII_PRIORITY_SPRITE_IN_FRONT) {
        sprite_wins = true;
    } else if (sprite->priority == VICII_PRIORITY_SPRITE_BEHIND) {
        sprite_wins = (current_priority <= VICII_PRIORITY_BACKGROUND);
    }
    
    if (sprite_wins) {
        vicii->pixel.pixel_line_priority[pixel_x] = sprite->priority;
        vicii->pixel.pixel_line_color[pixel_x] = sprite_color;
    }
}

// Sprite sequencer 
void vic_sprite_sequencer(vicii_common_t* vicii) {
    for (int i = VICII_NUM_SPRITES - 1; i >= 0; i--) {
        vic_sprite_emit_pixels(vicii, i);
    }
}

static void vic_pixel_sequencer(vicii_common_t* vicii) {
    uint16_t x_coord = vicii->timing.display_x_coordinate;
    
    // Check border flip-flops to determine display vs border
    bool in_display_window = !vicii->border.main_border_flip_flop && 
                            !vicii->border.vertical_border_flip_flop &&
                            x_coord >= vicii->border.border_left &&
                            x_coord <= vicii->border.border_right;
    
    if (in_display_window && vicii->video_logic.display_state) {
        // Graphics handled by c-access/g-access - sprites processed here
        vic_sprite_sequencer(vicii);
    } else {
        // Border area - emit border pixels at current x coordinate
        vic_pixel_emit_at_x(vicii, &vicii->border.border_pixel, x_coord);
    }
}

void vic_pixel_flush_line(vicii_common_t* vicii, uint32_t* palette, int y) {
    if (!vicii->pixel.framebuffer || !palette || y >= vicii->pixel.framebuffer_height) return;
    
    vic_pixel_unit_t* pixel = &vicii->pixel;  // Used multiple times - KEEP

    uint32_t* row_ptr = &pixel->framebuffer[y * pixel->framebuffer_width];
    uint32_t border_color = palette[vicii->registers.data[VICII_EC]]; // Direct access - single use
    
    // Fill entire line with border color
    for (int x = 0; x < pixel->framebuffer_width; x++) {
        row_ptr[x] = border_color;
    }
    
    // Copy VIC-II pixels
    int pixels_to_copy = (pixel->pixel_line_index < pixel->visible_pixels_per_line) ?
                        pixel->pixel_line_index : pixel->visible_pixels_per_line;
    
    if (pixel->pixel_line_color && pixels_to_copy > 0) {
        int offset_x = (pixel->framebuffer_width - pixel->visible_pixels_per_line) >> 1;
        
        for (int x = 0; x < pixels_to_copy; x++) {
            int fb_x = offset_x + x;
            if (fb_x >= 0 && fb_x < pixel->framebuffer_width) {
                uint8_t color_index = pixel->pixel_line_color[x] & 0x0F;
                row_ptr[fb_x] = palette[color_index];
            }
        }
    }
    
    pixel->pixel_line_index = 0;
}

static inline void vic_pixel_set_framebuffer(vic_pixel_unit_t* pixel, uint32_t* framebuffer, 
                                             int width, int height) {
    pixel->framebuffer = framebuffer;
    pixel->framebuffer_width = width;
    pixel->framebuffer_height = height;
}

// ========================================================================================
// SEQUENCER LOGIC
// ========================================================================================

static inline void vic_sequencer_update_colors(vicii_common_t* vicii) {
    vic_sequencer_unit_t* sequencer = &vicii->sequencer;
    vic_registers_unit_t* regs = &vicii->registers;
    
    // Update color palette based on graphics mode and background colors
    switch (sequencer->graphics_mode) {
        case VICII_GM_STANDARD_TEXT:
            sequencer->colors[0].color = regs->data[VICII_B0C];
            sequencer->colors[0].priority = VICII_PRIORITY_BACKGROUND;
            sequencer->colors[4].priority = VICII_PRIORITY_FOREGROUND;
            break;
        case VICII_GM_MULTICOLOR_TEXT:
            sequencer->colors[0].color = regs->data[VICII_B0C];
            sequencer->colors[1].color = regs->data[VICII_B1C];
            sequencer->colors[2].color = regs->data[VICII_B2C];
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
            sequencer->colors[0].color = regs->data[VICII_B0C];
            sequencer->colors[0].priority = VICII_PRIORITY_BACKGROUND;
            sequencer->colors[1].priority = VICII_PRIORITY_FOREGROUND;
            sequencer->colors[2].priority = VICII_PRIORITY_FOREGROUND;
            sequencer->colors[3].priority = VICII_PRIORITY_FOREGROUND;
            break;
        case VICII_GM_ECM_TEXT:
            sequencer->colors[0].color = regs->data[VICII_B0C];
            sequencer->colors[1].color = regs->data[VICII_B1C];
            sequencer->colors[2].color = regs->data[VICII_B2C];
            sequencer->colors[3].color = regs->data[VICII_B3C];
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

static inline void vic_sequencer_update_mode(vic_sequencer_unit_t* sequencer, uint8_t c1_reg, uint8_t c2_reg) {
    sequencer->graphics_mode = ((c1_reg & (VICII_C1_ECM | VICII_C1_BMM)) |
                               (c2_reg & VICII_C2_MCM)) >> 4;
}

// Timing functions
void vic_update_badline_condition(vicii_common_t* vicii) {
    uint16_t raster = vicii->timing.raster_counter;
    
    // "A Bad Line Condition is given at any arbitrary clock cycle, if at the
    // negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
    // <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
    // DEN bit was set during an arbitrary cycle of raster line $30."
    // Single range check instead of two comparisons
    if ((raster - 48) < 200) {  // Equivalent to raster >= 48 && raster < 248
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

static inline void vic_registers_write_interrupt(vic_registers_unit_t* regs, uint8_t value) {
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
    // IR_IRQ flag is untouched - it'll be cleared later, in vicii_common_handle_raster_interrupt()
}

// Register write function (uses all the above handlers)
void vic_registers_write(vicii_common_t* vicii, uint16_t address, uint8_t value) {
    uint8_t reg = address & VICII_REGS_MASK; // The VIC registers are repeated each 64 bytes in the area $d000-$d3ff
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
        vic_registers_write_interrupt(&vicii->registers, value);
        return;
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
            vic_update_badline_condition(vicii);
            // Fall through to C2 case
        case VICII_C2: // $d016 Control register 2
            vic_sequencer_update_mode(&vicii->sequencer, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
            vic_border_update_limits(&vicii->border, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
            break;
        case VICII_MXE: // $d015 Sprite enabled x
            // Update sprite enabled state
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vic_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
                sprite->enabled = (value & (1 << i)) != 0;
            }
            break;
        case VICII_MXYE: // $d017 Sprite Y expansion x
            // "Complex expansion flip flop logic per VIC-II documentation"
            // Optimized: set flip-flop state directly based on bit value
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vic_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
                // Writing 0 sets flip-flop, writing 1 clears it (immediate effect)
                // Note: cycle 55 inversion is handled separately in vicii_common_cycle()
                sprite->expansion_flip_flop = !(value & (1 << i));
            }
            break;
        case VICII_MP: // AI $d018 Memory pointers
            vic_memory_update_mapping(&vicii->memory, value);
            break;
        case VICII_MXDP: // $d01b Sprite data priority
            // Batch update sprite priorities
            for (int i = 0; i < VICII_NUM_SPRITES; i++) {
                vic_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
                sprite->priority = (value & (1 << i)) ? 
                    VICII_PRIORITY_SPRITE_BEHIND : VICII_PRIORITY_SPRITE_IN_FRONT;
            }
            break;
        case VICII_EC: // $d020 (4 bits) Exterior color (Border)
            vicii->border.border_pixel.color = value; // value already masked to 0x0F above
            // Fall through to B0C-B2C case
        case VICII_B0C: // $d021 (4 bits) Background color 0
        case VICII_B1C: // $d022 (4 bits) Background color 1
        case VICII_B2C: // $d023 (4 bits) Background color 2
            // Inline vic_border_update_color since priority is set only once during init
            vic_sequencer_update_colors(vicii);
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
}

// Register read helper (used by register read)
static inline uint8_t vic_read_clear(vic_registers_unit_t* regs, uint8_t reg) {
    uint8_t val = regs->data[reg];
    regs->data[reg] = 0;
    return val;
}

// Register read function (uses the above helper)
uint8_t vic_registers_read(vicii_common_t* vicii, uint16_t address) {
    uint8_t reg = address & VICII_REGS_MASK;
	// Used for "floating" bus state for subsequent unattached reads
    uint8_t data = ((c64_bus_t*)vicii->bus.bus)->data;
    
    // Fast path for most common registers
    switch (reg) {
        case VICII_C1:
            return (vicii->registers.data[VICII_C1] & 0x7F) |       //    17 $d011 Control register 1 
                   ((vicii->timing.raster_counter >> 1) & VICII_C1_RST8); //         bit 7 (RST8) reflects raster_counter bit 8 
        case VICII_RASTER:
            return vicii->timing.raster_counter & 0xFF;               //    18 $d012 Reflects raster_counter bits 0..7
        case VICII_C2:
            return vicii->registers.data[VICII_C2] | (data & 0xC0); //    22 $d016 |  - |  - | RES| MCM|CSEL|    XSCROLL   | Control register 2
        case VICII_MP:
            return vicii->registers.data[VICII_MP] | (data & 0x01); //    24 $d018 |VM13|VM12|VM11|VM10|CB13|CB12|CB11|  - | Memory pointers
        case VICII_IR:
            return vicii->registers.data[VICII_IR] | (data & 0x70); //    25 $d019 | IRQ|  - |  - |  - | ILP|IMMC|IMBC|IRST| Interrupt register
        case VICII_IE:
            return vicii->registers.data[VICII_IE] | (data & 0xF0); //    26 $d01a |  - |  - |  - |  - | ELP|EMMC|EMBC|ERST| Interrupt Enabled
        case VICII_MXM:
            return vic_read_clear(&vicii->registers, VICII_MXM_2);  //    30 $d01e Sprite-sprite collision is cleared on read
        case VICII_MXD:
            return vic_read_clear(&vicii->registers, VICII_MXD_2);  //    31 $d01f Sprite-data collision is cleared on read
        default:
		    if (reg <= 29) {
                return vicii->registers.data[reg];                  //  0-29 $d000-$d01f (except 22,24,25,26) use all 8 bits
 		   } else if (reg <= 46) {
        		return vicii->registers.data[reg] | (data & 0xF0);  // 32-46 $d020-$d02e use bits 0..3 (bits 4..7 are not connected)
		    } else {
		        return data;                                        // 47-63 $d02f-$d03f unattached registers (many docs say: give $ff on reading)
		    }
    }
}

// ========================================================================================
// TIMING AND VIDEO LOGIC
// ========================================================================================

// X coordinate calculation - now the primary counter
static inline void vic_update_timing_from_x_coordinate(vicii_common_t* vicii) {
    // Calculate cycle from x_coordinate (reverse of previous calculation)
    uint16_t adjusted_x = (vicii->timing.x_coordinate - vicii->timing.base_offset) & 0x1FF;
    vicii->timing.x_cycle = (uint8_t)(adjusted_x >> 3); // Divide by 8 pixels per cycle
    
    // Display coordinate with 12-pixel pipeline delay
    vicii->timing.display_x_coordinate = (vicii->timing.x_coordinate - 12) & 0x1FF;
}

void vic_timing_advance(vicii_common_t* vicii) {
    // Primary counter is now x_coordinate, advance by 1 pixel
    if (++vicii->timing.x_coordinate >= vicii->timing.pixels_per_line) {
        vicii->pixel.pixel_line_index = 0;
        vicii->timing.x_coordinate = 0;
        
        if (++vicii->timing.raster_counter >= vicii->timing.total_lines) {
            if (!vicii->pixel.framebuffer || vicii->timing.raster_counter >= vicii->pixel.framebuffer_height) {
                vicii->timing.raster_counter = 0;
                vicii->video_logic.was_den_set_during_raster_30 = false;
                vicii->video_logic.is_bad_line = false;
                vicii->video_logic.vcbase = 0;
            }
        }
        
        if (vicii->timing.raster_counter < 0x30 || vicii->timing.raster_counter > 0xf7) {
            vicii->video_logic.vcbase = 0;
        }
        
        if (vicii->timing.raster_counter == 0) {
            vicii->video_logic.refresh_counter = 0xFF;
        }
    }
    
    // Update derived cycle counter
    vic_update_timing_from_x_coordinate(vicii);
}

// ========================================================================================
// CYCLE FUNCTIONS
// ========================================================================================

static void vic_cycle_idle(vicii_common_t* vicii, int param) {
    vic_bus_control_ba_high(vicii);
    vic_bus_control_aec_high(vicii);
    vic_memory_access(vicii, VIC_ACCESS_IDLE, 0);
}

static void vic_cycle_sprite_p_access(vicii_common_t* vicii, int sprite_num) {
    vic_sprite_unit_t* sprite = &vicii->sprites.sprites[sprite_num];
    if (sprite->enabled) {
        vic_bus_control_ba_low(vicii);
        vic_bus_control_aec_low(vicii);
        vic_memory_access(vicii, VIC_ACCESS_P, (uint8_t)sprite_num);
    } else {
        vic_bus_control_ba_high(vicii);
        vic_bus_control_aec_high(vicii);
        vic_memory_access(vicii, VIC_ACCESS_IDLE, 0);
    }
}

static void vic_cycle_sprite_s_access(vicii_common_t* vicii, int sprite_num) {
    vic_sprite_unit_t* sprite = &vicii->sprites.sprites[sprite_num];
    if (sprite->enabled) {
        vic_bus_control_ba_low(vicii);
        vic_bus_control_aec_low(vicii);
        vic_memory_access(vicii, VIC_ACCESS_S, (uint8_t)sprite_num);
    } else {
        vic_bus_control_ba_high(vicii);
        vic_bus_control_aec_high(vicii);
        vic_memory_access(vicii, VIC_ACCESS_IDLE, 0);
    }
}

static void vic_cycle_refresh(vicii_common_t* vicii, int param) {
    vic_bus_control_ba_high(vicii);
    vic_bus_control_aec_high(vicii);
    vic_memory_access(vicii, VIC_ACCESS_REFRESH, 0);
}

static void vic_cycle_badline_setup(vicii_common_t* vicii, int param) {
    if (vicii->video_logic.is_bad_line) {
        vic_bus_control_ba_low(vicii);
        vicii->video_logic.display_state = true;
    } else {
        vic_bus_control_ba_high(vicii);
    }
    vic_bus_control_aec_high(vicii);
    vic_memory_access(vicii, VIC_ACCESS_IDLE, 0);
}

static void vic_cycle_vc_load(vicii_common_t* vicii, int param) {
    vicii->video_logic.vc = vicii->video_logic.vcbase;
    vicii->video_logic.vmli = 0;
    if (vicii->video_logic.is_bad_line) {
        vic_bus_control_ba_low(vicii);
        vicii->video_logic.display_state = true;
        vicii->video_logic.rc = 0;
    } else {
        vic_bus_control_ba_high(vicii);
    }
    vic_bus_control_aec_high(vicii);
    vic_memory_access(vicii, VIC_ACCESS_IDLE, 0);
}

static void vic_cycle_char_color_access(vicii_common_t* vicii, int char_index) {
    if (vicii->video_logic.is_bad_line) {
        vic_bus_control_ba_low(vicii);
        vic_bus_control_aec_low(vicii);
        vic_memory_access(vicii, VIC_ACCESS_C, (uint8_t)char_index);
        
        if (vicii->pixel.framebuffer && 
            vicii->timing.raster_counter < vicii->pixel.framebuffer_height &&
            vicii->timing.raster_counter >= vicii->border.border_top &&
            vicii->timing.raster_counter <= vicii->border.border_bottom) {
            vic_pixel_sequencer(vicii);
        }
    } else {
        vic_bus_control_ba_high(vicii);
        vic_bus_control_aec_high(vicii);
        vic_memory_access(vicii, VIC_ACCESS_G, (uint8_t)char_index);
        
        if (vicii->pixel.framebuffer && 
            vicii->timing.raster_counter < vicii->pixel.framebuffer_height) {
            vic_pixel_sequencer(vicii);
        }
    }
}

static void vic_cycle_sprite_p_expansion_check(vicii_common_t* vicii, int sprite_num) {
    // "7. In the first phase of cycle 16, it is checked if the expansion flip flop
    // is set. If so, MCBASE load from MC (MC->MCBASE), unless the CPU cleared
    // the Y expansion bit in $d017 in the second phase of cycle 15, in which case
    // MCBASE is set to X = (101010 & (MCBASE & MC)) | (010101 & (MCBASE | MC)).
    // After the MCBASE update, the VIC checks if MCBASE is equal to 63 and turns
    // off the DMA of the sprite if it is."
    
    uint8_t mxye_reg = vicii->registers.data[VICII_MXYE];
    for (int i = 0; i < VICII_NUM_SPRITES; i++) {
        if (mxye_reg & (1 << i)) {
            vic_sprite_unit_t* sprite = &vicii->sprites.sprites[i];
            sprite->expansion_flip_flop = !sprite->expansion_flip_flop;
            
            // Handle sprite crunch logic for MCBASE update
            if (sprite->expansion_flip_flop) {
                // Check if CPU cleared Y expansion bit in previous cycle (sprite crunch)
                // This is a simplified implementation - full crunch detection would require
                // tracking register writes within specific cycle phases
                // VIC-II Addendum sprite crunch logic:
                // MCBASE = (0xAA & (MCBASE & MC)) | (0x55 & (MCBASE | MC))
                uint8_t old_mcbase = sprite->mcbase;
                sprite->mcbase = (0xAA & (old_mcbase & sprite->mc)) | (0x55 & (old_mcbase | sprite->mc));
                // "After the MCBASE update, the VIC checks if MCBASE is equal to 63 and turns
                // off the DMA of the sprite if it is."
                if (sprite->mcbase == 63) {
                    sprite->dma_enabled = false;
                }
            }
        }
    }
    vic_cycle_sprite_p_access(vicii, sprite_num);
}

static void vic_cycle_sprite_s_rc_check(vicii_common_t* vicii, int sprite_num) {
    if (vicii->video_logic.rc == 7) {
        vicii->video_logic.display_state = false;
        vicii->video_logic.vcbase = vicii->video_logic.vc;
    }
    
    if (vicii->video_logic.display_state) {
        vicii->video_logic.rc++;
    }
    
    vic_cycle_sprite_s_access(vicii, sprite_num);
}

// Border Rules 2 & 3: Y coordinate checks in cycle 63 (1-based numbering)
// Combined with sprite S access for cycle efficiency
static void vic_cycle_sprite_s_border_check(vicii_common_t* vicii, int param) {
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
    
    // Perform the sprite S access for this cycle
    vic_cycle_sprite_s_access(vicii, param);
}

// ========================================================================================
// CYCLE TABLES
// ========================================================================================

// Cycle callback table - PAL timing (63 cycles per line) (Documentation section 3.6.3)
static const vic_cycle_entry_t vic_cycle_table_pal[63] = {
    {vic_cycle_idle, 0},                              // 1
    {vic_cycle_sprite_p_access, 0},                   // 2
    {vic_cycle_sprite_s_access, 0},                   // 3
    {vic_cycle_sprite_p_access, 1},                   // 4
    {vic_cycle_sprite_s_access, 1},                   // 5
    {vic_cycle_sprite_p_access, 2},                   // 6
    {vic_cycle_sprite_s_access, 2},                   // 7
    {vic_cycle_sprite_p_access, 3},                   // 8
    {vic_cycle_sprite_s_access, 3},                   // 9
    {vic_cycle_refresh, 0},                           // 10
    {vic_cycle_idle, 0},                              // 11
    {vic_cycle_idle, 0},                              // 12
    {vic_cycle_badline_setup, 0},                     // 13
    {vic_cycle_badline_setup, 0},                     // 14
    {vic_cycle_vc_load, 0},                           // 15
    {vic_cycle_char_color_access, 0},                 // 16
    {vic_cycle_char_color_access, 1},                 // 17
    {vic_cycle_char_color_access, 2},                 // 18
    {vic_cycle_char_color_access, 3},                 // 19
    {vic_cycle_char_color_access, 4},                 // 20
    {vic_cycle_char_color_access, 5},                 // 21
    {vic_cycle_char_color_access, 6},                 // 22
    {vic_cycle_char_color_access, 7},                 // 23
    {vic_cycle_char_color_access, 8},                 // 24
    {vic_cycle_char_color_access, 9},                 // 25
    {vic_cycle_char_color_access, 10},                // 26
    {vic_cycle_char_color_access, 11},                // 27
    {vic_cycle_char_color_access, 12},                // 28
    {vic_cycle_char_color_access, 13},                // 29
    {vic_cycle_char_color_access, 14},                // 30
    {vic_cycle_char_color_access, 15},                // 31
    {vic_cycle_char_color_access, 16},                // 32
    {vic_cycle_char_color_access, 17},                // 33
    {vic_cycle_char_color_access, 18},                // 34
    {vic_cycle_char_color_access, 19},                // 35
    {vic_cycle_char_color_access, 20},                // 36
    {vic_cycle_char_color_access, 21},                // 37
    {vic_cycle_char_color_access, 22},                // 38
    {vic_cycle_char_color_access, 23},                // 39
    {vic_cycle_char_color_access, 24},                // 40
    {vic_cycle_char_color_access, 25},                // 41
    {vic_cycle_char_color_access, 26},                // 42
    {vic_cycle_char_color_access, 27},                // 43
    {vic_cycle_char_color_access, 28},                // 44
    {vic_cycle_char_color_access, 29},                // 45
    {vic_cycle_char_color_access, 30},                // 46
    {vic_cycle_char_color_access, 31},                // 47
    {vic_cycle_char_color_access, 32},                // 48
    {vic_cycle_char_color_access, 33},                // 49
    {vic_cycle_char_color_access, 34},                // 50
    {vic_cycle_char_color_access, 35},                // 51
    {vic_cycle_char_color_access, 36},                // 52
    {vic_cycle_char_color_access, 37},                // 53
    {vic_cycle_char_color_access, 38},                // 54
    {vic_cycle_char_color_access, 39},                // 55
    {vic_cycle_sprite_p_expansion_check, 4},          // 56
    {vic_cycle_sprite_s_access, 4},                   // 57
    {vic_cycle_sprite_p_access, 5},                   // 58
    {vic_cycle_sprite_s_rc_check, 5},                 // 59
    {vic_cycle_sprite_p_access, 6},                   // 60
    {vic_cycle_sprite_s_access, 6},                   // 61
    {vic_cycle_sprite_p_access, 7},                   // 62
    {vic_cycle_sprite_s_border_check, 7}              // 63
};

// Cycle callback table - NTSC timing (65 cycles per line)
static const vic_cycle_entry_t vic_cycle_table_ntsc[65] = {
    {vic_cycle_idle, 0},                              // 1
    {vic_cycle_sprite_p_access, 0},                   // 2
    {vic_cycle_sprite_s_access, 0},                   // 3
    {vic_cycle_sprite_p_access, 1},                   // 4
    {vic_cycle_sprite_s_access, 1},                   // 5
    {vic_cycle_sprite_p_access, 2},                   // 6
    {vic_cycle_sprite_s_access, 2},                   // 7
    {vic_cycle_sprite_p_access, 3},                   // 8
    {vic_cycle_sprite_s_access, 3},                   // 9
    {vic_cycle_refresh, 0},                           // 10
    {vic_cycle_idle, 0},                              // 11
    {vic_cycle_idle, 0},                              // 12
    {vic_cycle_badline_setup, 0},                     // 13
    {vic_cycle_badline_setup, 0},                     // 14
    {vic_cycle_vc_load, 0},                           // 15
    {vic_cycle_char_color_access, 0},                 // 16
    {vic_cycle_char_color_access, 1},                 // 17
    {vic_cycle_char_color_access, 2},                 // 18
    {vic_cycle_char_color_access, 3},                 // 19
    {vic_cycle_char_color_access, 4},                 // 20
    {vic_cycle_char_color_access, 5},                 // 21
    {vic_cycle_char_color_access, 6},                 // 22
    {vic_cycle_char_color_access, 7},                 // 23
    {vic_cycle_char_color_access, 8},                 // 24
    {vic_cycle_char_color_access, 9},                 // 25
    {vic_cycle_char_color_access, 10},                // 26
    {vic_cycle_char_color_access, 11},                // 27
    {vic_cycle_char_color_access, 12},                // 28
    {vic_cycle_char_color_access, 13},                // 29
    {vic_cycle_char_color_access, 14},                // 30
    {vic_cycle_char_color_access, 15},                // 31
    {vic_cycle_char_color_access, 16},                // 32
    {vic_cycle_char_color_access, 17},                // 33
    {vic_cycle_char_color_access, 18},                // 34
    {vic_cycle_char_color_access, 19},                // 35
    {vic_cycle_char_color_access, 20},                // 36
    {vic_cycle_char_color_access, 21},                // 37
    {vic_cycle_char_color_access, 22},                // 38
    {vic_cycle_char_color_access, 23},                // 39
    {vic_cycle_char_color_access, 24},                // 40
    {vic_cycle_char_color_access, 25},                // 41
    {vic_cycle_char_color_access, 26},                // 42
    {vic_cycle_char_color_access, 27},                // 43
    {vic_cycle_char_color_access, 28},                // 44
    {vic_cycle_char_color_access, 29},                // 45
    {vic_cycle_char_color_access, 30},                // 46
    {vic_cycle_char_color_access, 31},                // 47
    {vic_cycle_char_color_access, 32},                // 48
    {vic_cycle_char_color_access, 33},                // 49
    {vic_cycle_char_color_access, 34},                // 50
    {vic_cycle_char_color_access, 35},                // 51
    {vic_cycle_char_color_access, 36},                // 52
    {vic_cycle_char_color_access, 37},                // 53
    {vic_cycle_char_color_access, 38},                // 54
    {vic_cycle_char_color_access, 39},                // 55
    {vic_cycle_sprite_p_expansion_check, 4},          // 56
    {vic_cycle_sprite_s_access, 4},                   // 57
    {vic_cycle_sprite_p_access, 5},                   // 58
    {vic_cycle_sprite_s_rc_check, 5},                 // 59
    {vic_cycle_sprite_p_access, 6},                   // 60
    {vic_cycle_sprite_s_access, 6},                   // 61
    {vic_cycle_sprite_p_access, 7},                   // 62
    {vic_cycle_sprite_s_border_check, 7},             // 63
    {vic_cycle_idle, 0},                              // 64
    {vic_cycle_idle, 0}                               // 65
};

// ========================================================================================
// MAIN CYCLE FUNCTION
// ========================================================================================

// Border flip-flop logic (Documentation section 3.9) - X coordinate rules only
static inline void vic_border_update_flip_flops_x(vic_border_unit_t* border, vic_timing_unit_t* timing, 
                                                  uint8_t c1_reg) {
    uint16_t raster = timing->raster_counter;
    uint16_t x_coord = timing->display_x_coordinate;
    bool den_set = (c1_reg & VICII_C1_DEN) != 0;
    
    // "The flip flops are switched according to the following rules:"
    
    // Rule 1: "If the X coordinate reaches the right comparison value, the main border flip flop is set."
    if (x_coord == border->border_right) {
        border->main_border_flip_flop = true;
    }
    
    // Rules 4, 5, 6: Handle left coordinate checks only
    else if (x_coord == border->border_left) {
        // Rule 4: "If the X coordinate reaches the left comparison value and the Y
        // coordinate reaches the bottom one, the vertical border flip flop is set."
        if (raster == border->border_bottom) {
            border->vertical_border_flip_flop = true;
        }
        // Rule 5: "If the X coordinate reaches the left comparison value and the Y
        // coordinate reaches the top one and the DEN bit in register $d011 is set,
        // the vertical border flip flop is reset."
        else if (raster == border->border_top && den_set) {
            border->vertical_border_flip_flop = false;
        }
        
        // Rule 6: "If the X coordinate reaches the left comparison value and the vertical
        // border flip flop is not set, the main flip flop is reset."
        if (!border->vertical_border_flip_flop) {
            border->main_border_flip_flop = false;
        }
    }
}

void vicii_common_cycle(vicii_common_t* vicii) {
    // Get current cycle entry (derived from x_coordinate)
    const vic_cycle_entry_t* entry = &vicii->timing.cycle_table[vicii->timing.x_cycle];
    entry->func(vicii, entry->param);
    
    // Update border flip-flops AFTER pixel generation using hardware-accurate coordinates
    vic_border_update_flip_flops_x(&vicii->border, &vicii->timing, 
                                  vicii->registers.data[VICII_C1]);
    
    // Advance x_coordinate (primary counter) and update derived values
    vic_timing_advance(vicii);
    vic_update_badline_condition(vicii);
    
    // Flush pixel line if end of line
    if (vicii->timing.x_coordinate == 0 && vicii->pixel.framebuffer) {
        vic_pixel_flush_line(vicii, vicii_common_get_default_palette(),
                           (vicii->timing.raster_counter - 1) % vicii->timing.total_lines);
    }
}

// ========================================================================================
// INITIALIZATION
// ========================================================================================

static inline void vicii_common_initialize(vicii_common_t* vicii) {
    // Zero all units
    memset(&vicii->registers, 0, sizeof(vic_registers_unit_t));
    memset(&vicii->timing, 0, sizeof(vic_timing_unit_t));
    memset(&vicii->video_logic, 0, sizeof(vic_video_logic_unit_t));
    memset(&vicii->video_data, 0, sizeof(vic_video_data_unit_t));
    memset(&vicii->sequencer, 0, sizeof(vic_sequencer_unit_t));
    memset(&vicii->border, 0, sizeof(vic_border_unit_t));
    memset(&vicii->memory, 0, sizeof(vic_memory_unit_t));
    memset(&vicii->sprites, 0, sizeof(vic_sprites_unit_t));
    memset(&vicii->pixel, 0, sizeof(vic_pixel_unit_t));
    memset(&vicii->bus, 0, sizeof(vic_bus_unit_t));
    
    // Set default register values
    vicii->registers.data[VICII_C1] = VICII_C1_RST8 | VICII_C1_DEN | VICII_C1_RSEL |
                            (VICII_C1_YSCROLL & 3); // 155:Display ENable,25-row  
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
    vic_sequencer_update_mode(&vicii->sequencer, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
    vic_border_update_limits(&vicii->border, vicii->registers.data[VICII_C1], vicii->registers.data[VICII_C2]);
    // Initialize border priority once (color will be updated by register writes)
    vicii->border.border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border.border_pixel.color = vicii->registers.data[VICII_EC];
    // Initialize border flip-flops (Documentation section 3.9)
    vicii->border.main_border_flip_flop = true;      // Start with border on
    vicii->border.vertical_border_flip_flop = true;  // Start with vertical border on
    vic_memory_update_mapping(&vicii->memory, vicii->registers.data[VICII_MP]);
    
    // Initialize refresh counter (Documentation section 3.13)
    vicii->video_logic.refresh_counter = 0xFF;
    
    // Set timing defaults (should be set by wrapper create functions)
    vicii->timing.cycles_per_line = VICII_PAL_CYCLES_PER_LINE;
    vicii->timing.total_lines = VICII_PAL_TOTAL_LINES;
    vicii->pixel.visible_pixels_per_line = VICII_PAL_VISIBLE_PIXELS;
    
    // Allocate pixel buffers
    if (vicii->pixel.visible_pixels_per_line > 0) {
        vicii->pixel.pixel_line_priority = malloc(vicii->pixel.visible_pixels_per_line * sizeof(vicii_priority_t));
        vicii->pixel.pixel_line_color = malloc(vicii->pixel.visible_pixels_per_line * sizeof(uint32_t));
        
        memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, vicii->pixel.visible_pixels_per_line);
        for (int i = 0; i < vicii->pixel.visible_pixels_per_line; i++) {
            vicii->pixel.pixel_line_color[i] = VICII_COLOR_LIGHT_BLUE;
        }
    }
}

static inline void vicii_common_initialize_timing(vicii_common_t* vicii, bool is_pal) {
    if (is_pal) {
        vicii->timing.cycles_per_line = VICII_PAL_CYCLES_PER_LINE;
        vicii->timing.total_lines = VICII_PAL_TOTAL_LINES;
        vicii->timing.cycle_table = vic_cycle_table_pal;
        vicii->timing.base_offset = 404;
        vicii->timing.pixels_per_line = 504; // 63 cycles * 8 pixels per cycle
        vicii->pixel.visible_pixels_per_line = VICII_PAL_VISIBLE_PIXELS;
    } else {
        vicii->timing.cycles_per_line = VICII_NTSC_CYCLES_PER_LINE;
        vicii->timing.total_lines = VICII_NTSC_TOTAL_LINES;
        vicii->timing.cycle_table = vic_cycle_table_ntsc;
        vicii->timing.base_offset = 412;
        vicii->timing.pixels_per_line = 520; // 65 cycles * 8 pixels per cycle
        vicii->pixel.visible_pixels_per_line = VICII_NTSC_VISIBLE_PIXELS;
    }
    
    vicii->timing.x_cycle = 0;
    vicii->timing.raster_counter = 0;
}

// ========================================================================================
// PUBLIC API FUNCTIONS
// ========================================================================================

vicii_common_t* vicii_common_system_create(chip_descriptor_t* desc, void (*bank_change)(void*, uint8_t)) {
    vicii_common_t* vicii = (vicii_common_t*)calloc(1, sizeof(vicii_common_t));
    if (!vicii) return NULL;
    
    vicii->desc = desc;
    vicii->bus.bank_change = bank_change;
    vicii_common_initialize(vicii);
    
    return vicii;
}

void vicii_common_system_destroy(void* chip) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    if (vicii) {
        free(vicii->pixel.pixel_line_priority);
        free(vicii->pixel.pixel_line_color);
        free(vicii);
    }
}

void vicii_common_bus_attach(void* chip, void* bus) {
    ((vicii_common_t*)chip)->bus.bus = bus;
}

uint8_t vicii_common_registers_read(void* chip, uint16_t address) {
    return vic_registers_read((vicii_common_t*)chip, address);
}

void vicii_common_registers_write(void* chip, uint16_t address, uint8_t value) {
    vic_registers_write((vicii_common_t*)chip, address, value);
}

void vicii_common_bank_change(void* chip, uint8_t bank) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    bank = 3 - (bank & 0x03);  // Invert bank
    vicii->memory.bank = bank;
    vicii->memory.bank_base = bank * 0x4000;
}

void vicii_common_set_framebuffer(vicii_common_t* vicii, uint32_t* framebuffer, int width, int height) {
    vic_pixel_set_framebuffer(&vicii->pixel, framebuffer, width, height);
    
    vicii->border.border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border.border_pixel.color = VICII_COLOR_LIGHT_BLUE;
    
    if (vicii->pixel.pixel_line_color && vicii->pixel.visible_pixels_per_line > 0) {
        memset(vicii->pixel.pixel_line_priority, VICII_PRIORITY_BORDER, vicii->pixel.visible_pixels_per_line);
        for (int i = 0; i < vicii->pixel.visible_pixels_per_line; i++) {
            vicii->pixel.pixel_line_color[i] = VICII_COLOR_LIGHT_BLUE;
        }
    }
}
