#include "vicii_common.h"
#include "../../../systems/c64/c64_bus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper function to read clear (collision registers)
static uint8_t read_clear(vicii_common_t* vicii, uint8_t reg) {
    uint8_t val = vicii->registers[reg];
    vicii->registers[reg] = 0;
    return val;
}

// Helper function to set main border flip flop
static void set_main_border_flip_flop(vicii_common_t* vicii, bool main_border_flip_flop) {
    // Always set border pixel state to use border color regardless of flip-flop state
    // The actual border rendering will be handled by the cycle function
    vicii->border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border_pixel.color = vicii->registers[VICII_EC] & 0x0F;
    
    // Update pixel line buffers if they exist
    if (vicii->pixel_line_color && vicii->visible_pixels_per_line > 0) {
        uint8_t border_color = vicii->registers[VICII_EC] & 0x0F;
        for (int i = 0; i < vicii->visible_pixels_per_line; i++) {
            vicii->pixel_line_priority[i] = VICII_PRIORITY_BORDER;
            vicii->pixel_line_color[i] = border_color;
        }
    }
}

// Update graphics mode and dependent colors
static void update_graphics_mode_and_dependent_colors(vicii_common_t* vicii) {
    // Update the graphics mode
    vicii->graphics_mode = ((vicii->registers[VICII_C1] & (VICII_C1_ECM | VICII_C1_BMM)) |
                           (vicii->registers[VICII_C2] & VICII_C2_MCM)) >> 4;
    
    // Update border limits
    vicii->border_top = (vicii->registers[VICII_C1] & VICII_C1_RSEL) == 0 ?
                        VICII_BORDER_TOP_RSEL0 : VICII_BORDER_TOP_RSEL1;
    vicii->border_bottom = (vicii->registers[VICII_C1] & VICII_C1_RSEL) == 0 ?
                           VICII_BORDER_BOTTOM_RSEL0 : VICII_BORDER_BOTTOM_RSEL1;
    vicii->border_left = (vicii->registers[VICII_C2] & VICII_C2_CSEL) == 0 ?
                         VICII_BORDER_LEFT_CSEL0 : VICII_BORDER_LEFT_CSEL1;
    vicii->border_right = (vicii->registers[VICII_C2] & VICII_C2_CSEL) == 0 ?
                          VICII_BORDER_RIGHT_CSEL0 : VICII_BORDER_RIGHT_CSEL1;
    
    // Update colors based on graphics mode
    switch (vicii->graphics_mode) {
        case VICII_GM_STANDARD_TEXT:
            vicii->colors[0].color = vicii->registers[VICII_B0C] & 0x0F;
            break;
        case VICII_GM_MULTICOLOR_TEXT:
            vicii->colors[0].color = vicii->registers[VICII_B0C] & 0x0F;
            vicii->colors[1].color = vicii->registers[VICII_B1C] & 0x0F;
            vicii->colors[2].color = vicii->registers[VICII_B2C] & 0x0F;
            break;
        case VICII_GM_MULTICOLOR_BITMAP:
            vicii->colors[0].color = vicii->registers[VICII_B0C] & 0x0F;
            break;
        case VICII_GM_INVALID_TEXT:
        case VICII_GM_INVALID_BITMAP1:
        case VICII_GM_INVALID_BITMAP2:
            // Invalid modes show black
            for (int i = 0; i < 5; i++) {
                vicii->colors[i].color = VICII_COLOR_BLACK;
            }
            break;
    }
}

// Update bad line condition
static void update_bad_line(vicii_common_t* vicii) {
    bool eevmf = (vicii->raster_counter >= 48) && (vicii->raster_counter < 248);
    
    if (eevmf) {
        // Check if DEN was set during raster line $30
        if (vicii->raster_counter == 0x30) {
            if (!vicii->was_den_set_during_raster_30) {
                vicii->was_den_set_during_raster_30 =
                    (vicii->registers[VICII_C1] & VICII_C1_DEN) != 0;
            }
        }
        
        vicii->bad_line = vicii->was_den_set_during_raster_30 &&
                         ((vicii->raster_counter & 0x07) ==
                          (vicii->registers[VICII_C1] & VICII_C1_YSCROLL));
    } else {
        vicii->bad_line = false;
    }
}

// Handle raster interrupt
static void handle_raster_interrupt(vicii_common_t* vicii) {
    // Raster interrupt line reached?
    uint16_t raster_compare = ((vicii->registers[VICII_C1] & VICII_C1_RST8) << 1) |
                              vicii->registers[VICII_RASTER];
    
    if (vicii->raster_counter == raster_compare) {
        vicii->registers[VICII_IR] |= VICII_IR_IRST;
    }
    
    // Check if any interrupt is both signaled and enabled
    uint8_t interrupt_status = vicii->registers[VICII_IR] & vicii->registers[VICII_IE];
    if ((interrupt_status & VICII_INTERRUPTS_MASK) > 0) {
        vicii->registers[VICII_IR] |= VICII_IR_IRQ;
        // TODO: Signal IRQ to CPU via bus
    } else {
        if ((vicii->registers[VICII_IR] & VICII_IR_IRQ) > 0) {
            vicii->registers[VICII_IR] &= ~VICII_IR_IRQ;
        }
    }
}

// VIC-II system creation
vicii_common_t* vicii_common_system_create(chip_descriptor_t* desc, void (*bank_change)(void*, uint8_t)) {
    vicii_common_t* vicii = (vicii_common_t*)calloc(1, sizeof(vicii_common_t));
    if (!vicii) return NULL;
    
    vicii->desc = desc;
    vicii->bank_change = bank_change;
    
    // Initialize registers to default values
    vicii_common_initialize(vicii);
    
    return vicii;
}

void vicii_common_system_destroy(void* chip) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    if (vicii) {
        if (vicii->pixel_line_priority) free(vicii->pixel_line_priority);
        if (vicii->pixel_line_color) free(vicii->pixel_line_color);
        free(vicii);
    }
}

void vicii_common_bus_attach(void* chip, void* bus) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    vicii->bus = bus;
}

// Register read with proper masking
uint8_t vicii_common_registers_read(void* chip, uint16_t address) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    uint8_t reg = address & VICII_REGS_MASK;
    
    switch (reg) {
        case VICII_C1:
            return (vicii->registers[VICII_C1] & 0x7F) |
                   ((vicii->raster_counter >> 1) & VICII_C1_RST8);
        case VICII_RASTER:
            return vicii->raster_counter & 0xFF;
        case VICII_C2:
            return vicii->registers[VICII_C2] | 0xC0;
        case VICII_MP:
            return vicii->registers[VICII_MP] | 0x01;
        case VICII_IE:
            return vicii->registers[VICII_IE] | 0xF0;
        case VICII_MXM:
            return read_clear(vicii, VICII_REGS_SIZE);  // Shadow register
        case VICII_MXD:
            return read_clear(vicii, VICII_REGS_SIZE + 1);  // Shadow register
        default:
            if (reg <= 29) {
                return vicii->registers[reg];
            } else if (reg >= 32 && reg <= 46) {
                return vicii->registers[reg] | 0xF0;  // Color registers use only 4 bits
            } else if (reg >= 47) {
                return 0xFF;  // Unused addresses
            }
            return vicii->registers[reg];
    }
}

// Register write with proper handling
void vicii_common_registers_write(void* chip, uint16_t address, uint8_t value) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    uint8_t reg = address & VICII_REGS_MASK;
    
    // Handle interrupt register specially
    if (reg == VICII_IR) {
        // Clear interrupt bits that are set in value
        value &= VICII_INTERRUPTS_MASK;
        uint8_t ir = vicii->registers[VICII_IR];
        ir &= ~value;
        ir |= VICII_IR_UNUSED;
        vicii->registers[VICII_IR] = ir;
        return;
    }
    
    // Mask color registers to 4 bits
    if (reg >= VICII_EC) {
        value &= 0x0F;
    }
    
    vicii->registers[reg] = value;
    
    // Handle register-specific updates
    switch (reg) {
        case VICII_C1:
            update_bad_line(vicii);
            update_graphics_mode_and_dependent_colors(vicii);
            break;
        case VICII_C2:
            update_graphics_mode_and_dependent_colors(vicii);
            break;
        case VICII_MP:
            // Memory pointers changed - update memory mapping
            vicii_update_bank_mapping(vicii, vicii->bank);
            break;
        case VICII_EC:
            set_main_border_flip_flop(vicii,
                vicii->border_pixel.priority > VICII_PRIORITY_BACKGROUND);
            break;
        case VICII_B0C:
            set_main_border_flip_flop(vicii,
                vicii->border_pixel.priority > VICII_PRIORITY_BACKGROUND);
            update_graphics_mode_and_dependent_colors(vicii);
            break;
        case VICII_B1C:
        case VICII_B2C:
            update_graphics_mode_and_dependent_colors(vicii);
            break;
    }
}

void vicii_common_bank_change(void* chip, uint8_t bank) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    vicii_update_bank_mapping(vicii, bank);
}

// Initialize VIC-II to default state
void vicii_common_initialize(vicii_common_t* vicii) {
    // Set registers to default values
    for (int r = 0; r < VICII_REGS_SIZE; r++) {
        switch (r) {
            case VICII_C1:
                vicii->registers[r] = VICII_C1_RST8 | VICII_C1_DEN | VICII_C1_RSEL |
                                     (VICII_C1_YSCROLL & 3);
                break;
            case VICII_MXE:
                vicii->registers[r] = 0;  // All sprites disabled
                break;
            case VICII_C2:
                vicii->registers[r] = VICII_C2_CSEL;
                break;
            case VICII_IR:
                vicii->registers[r] = VICII_IR_UNUSED;
                break;
            case VICII_MP:
                vicii->registers[r] = VICII_MP_CB12 | VICII_MP_VM10;
                break;
            case VICII_EC:
                vicii->registers[r] = VICII_COLOR_LIGHT_BLUE;
                break;
            case VICII_B0C:
                vicii->registers[r] = VICII_COLOR_BLUE;
                break;
            case VICII_B1C:
                vicii->registers[r] = VICII_COLOR_WHITE;
                break;
            case VICII_B2C:
                vicii->registers[r] = VICII_COLOR_RED;
                break;
            case VICII_B3C:
                vicii->registers[r] = VICII_COLOR_CYAN;
                break;
            case VICII_MM0:
                vicii->registers[r] = VICII_COLOR_PURPLE;
                break;
            case VICII_MM1:
                vicii->registers[r] = VICII_COLOR_BLACK;
                break;
            case VICII_M0C:
                vicii->registers[r] = VICII_COLOR_WHITE;
                break;
            case VICII_M1C:
                vicii->registers[r] = VICII_COLOR_RED;
                break;
            case VICII_M2C:
                vicii->registers[r] = VICII_COLOR_CYAN;
                break;
            case VICII_M3C:
                vicii->registers[r] = VICII_COLOR_PURPLE;
                break;
            case VICII_M4C:
                vicii->registers[r] = VICII_COLOR_GREEN;
                break;
            case VICII_M5C:
                vicii->registers[r] = VICII_COLOR_BLUE;
                break;
            case VICII_M6C:
                vicii->registers[r] = VICII_COLOR_YELLOW;
                break;
            case VICII_M7C:
                vicii->registers[r] = VICII_COLOR_MEDIUM_GREY;
                break;
            default:
                if (r >= 47) {
                    vicii->registers[r] = 0xFF;  // Unused addresses
                } else {
                    vicii->registers[r] = 0;
                }
                break;
        }
    }
    
    // Initialize color priorities
    vicii->colors[0].priority = VICII_PRIORITY_BACKGROUND;
    vicii->colors[1].priority = VICII_PRIORITY_BACKGROUND;
    vicii->colors[2].priority = VICII_PRIORITY_FOREGROUND;
    vicii->colors[3].priority = VICII_PRIORITY_FOREGROUND;
    vicii->colors[4].priority = VICII_PRIORITY_FOREGROUND;
    
    update_graphics_mode_and_dependent_colors(vicii);
    
    // Initialize border generation properly
    set_main_border_flip_flop(vicii, true);  // Start generating border pixels
    
    // Initialize VIC-II memory mapping to default bank 0
    vicii_update_bank_mapping(vicii, 0);
    
    // Initialize video logic display state to start generating pixels
    vicii->video_logic_display_state = false;  // Will be enabled during bad lines
    
    // Set visible pixels per line based on timing
    if (vicii->visible_pixels_per_line == 0) {
        // Default to PAL size if not set - this should be set by wrapper create functions
        vicii->visible_pixels_per_line = VICII_PAL_VISIBLE_PIXELS;
    }
    
    // Allocate pixel buffers
    if (vicii->visible_pixels_per_line > 0) {
        vicii->pixel_line_priority = malloc(vicii->visible_pixels_per_line * sizeof(vicii_priority_t));
        vicii->pixel_line_color = malloc(vicii->visible_pixels_per_line * sizeof(uint32_t));
        
        // Initialize pixel buffers with default values (light blue border)
        for (int i = 0; i < vicii->visible_pixels_per_line; i++) {
            vicii->pixel_line_priority[i] = VICII_PRIORITY_BORDER;
            vicii->pixel_line_color[i] = VICII_COLOR_LIGHT_BLUE;  // Default VIC-II border color
        }
    }
}

// VIC-II specific memory read function with proper banking
uint8_t vicii_memory_read_cycle(vicii_common_t* vicii, uint16_t address) {
    if (!vicii->bus) return 0xFF;
    
    // VIC-II only sees 14-bit addresses (16KB banks)
    // The top 2 bits come from CIA2 port A (inverted)
    uint16_t vic_address = (address & 0x3FFF) | vicii->memory_map.bank_base;
    
    // Special handling for character ROM access
    // Character ROM is visible in VIC bank when:
    // 1. Address is in range $1000-$1FFF or $9000-$9FFF
    // 2. Character ROM is enabled (determined by memory setup register)
    if (vicii->memory_map.char_rom_enabled) {
        uint16_t char_check = address & 0x3000;
        if (char_check == 0x1000 || char_check == 0x9000) {
            // Access character ROM directly (bypass banking)
            c64_bus_t* bus = (c64_bus_t*)vicii->bus;
            return bus->read_callbacks[ACID_CHARROM].read(
                bus->read_callbacks[ACID_CHARROM].context,
                (address & 0x0FFF) | 0xD000  // Map to $D000-$DFFF range
            );
        }
    }
    
    // Use bus memory read for all other accesses
    return c64_bus_memory_read((c64_bus_t*)vicii->bus, vic_address);
}

// Update VIC-II bank mapping when CIA2 changes the bank
void vicii_update_bank_mapping(vicii_common_t* vicii, uint8_t bank) {
    // VIC-II bank is inverted: 0=bank3, 1=bank2, 2=bank1, 3=bank0
    bank = 3 - (bank & 0x03);
    vicii->bank = bank;
    
    switch (bank) {
        case 0: vicii->memory_map.bank_base = VICII_BANK_0_BASE; break;
        case 1: vicii->memory_map.bank_base = VICII_BANK_1_BASE; break;
        case 2: vicii->memory_map.bank_base = VICII_BANK_2_BASE; break;
        case 3: vicii->memory_map.bank_base = VICII_BANK_3_BASE; break;
    }
    
    // Update video matrix and character base addresses
    uint8_t mp_reg = vicii->registers[VICII_MP];
    vicii->memory_map.video_matrix_base = ((mp_reg & 0xF0) >> 4) * 0x400;
    vicii->memory_map.char_base = ((mp_reg & 0x0E) >> 1) * 0x800;
    
    // Character ROM is accessible in banks 0 and 2 when char_base points to $1000/$9000
    vicii->memory_map.char_rom_enabled = (bank == 0 || bank == 2) &&
                                        (vicii->memory_map.char_base == 0x1000 ||
                                         vicii->memory_map.char_base == 0x9000);
}

// Character access (c-access) - reads from video matrix
void vicii_common_c_access(vicii_common_t* vicii) {
    if (!vicii->video_logic_display_state) return;
    
    // Calculate video matrix address
    uint16_t address = vicii->memory_map.video_matrix_base | vicii->vc;
    
    // Read character code from video matrix
    vicii->video_matrix_line[vicii->vmli] = vicii_memory_read_cycle(vicii, address);
    
    // Read color from color RAM (always at $D800-$DBFF, independent of VIC banking)
    if (vicii->bus) {
        c64_bus_t* bus = (c64_bus_t*)vicii->bus;
        vicii->video_color_line[vicii->vmli] =
            bus->read_callbacks[ACID_COLORRAM_D8].read(
                bus->read_callbacks[ACID_COLORRAM_D8].context,
                0xD800 + vicii->vc
            ) & 0x0F;  // Color RAM is only 4 bits
    }
}

// Graphics access (g-access) - reads character/bitmap data
void vicii_common_g_access(vicii_common_t* vicii) {
    uint16_t address;
    uint8_t char_code = 0;
    vicii_color_t color_code = VICII_COLOR_BLACK;
    
    if (vicii->video_logic_display_state) {
        // Get data from video matrix line (set by c-access)
        char_code = vicii->video_matrix_line[vicii->vmli];
        color_code = vicii->video_color_line[vicii->vmli];
        
        // Calculate graphics data address based on mode
        if ((vicii->graphics_mode & VICII_BITMAP_MODE_MASK) == 0) {
            // Text mode: address = char_base + (char_code * 8) + row
            address = vicii->memory_map.char_base + (char_code << 3) + vicii->rc;
        } else {
            // Bitmap mode: address = char_base + (vc * 8) + row
            address = vicii->memory_map.char_base + (vicii->vc << 3) + vicii->rc;
        }
    } else {
        // Idle state: always access $3fff
        address = 0x3fff;
    }
    
    // Handle Extended Color Mode (ECM)
    if (vicii->graphics_mode & VICII_EXTENDED_COLOR_MODE_MASK) {
        // In ECM, address lines 9 and 10 are forced low
        address &= ~0x0600;
    }
    
    // Read graphics data
    uint8_t graphics_data = vicii_memory_read_cycle(vicii, address);
    
    // Update colors based on graphics mode and character/color data
    switch (vicii->graphics_mode) {
        case VICII_GM_STANDARD_TEXT:
            vicii->colors[4].color = color_code;
            break;
            
        case VICII_GM_MULTICOLOR_TEXT: {
            uint8_t mc_flag = (color_code >> 3) & 1;  // Bit 3 determines multicolor
            if (mc_flag) {
                vicii->colors[3].color = color_code & 0x07;  // Use bits 0-2
            } else {
                vicii->colors[4].color = color_code;
            }
            break;
        }
        
        case VICII_GM_STANDARD_BITMAP:
            vicii->colors[0].color = char_code & 0x0F;       // Lower nibble
            vicii->colors[4].color = (char_code >> 4) & 0x0F; // Upper nibble
            break;
            
        case VICII_GM_MULTICOLOR_BITMAP:
            vicii->colors[1].color = (char_code >> 4) & 0x0F; // Upper nibble
            vicii->colors[2].color = char_code & 0x0F;        // Lower nibble
            vicii->colors[3].color = color_code;              // Color RAM
            break;
            
        case VICII_GM_ECM_TEXT:
            vicii->colors[0].color = vicii->registers[VICII_B0C + ((char_code >> 6) & 3)] & 0x0F;
            vicii->colors[4].color = color_code;
            break;
    }
    
    // Emit pixels based on graphics data and mode
    vicii_common_emit_graphics_pixels(vicii, graphics_data);
    
    // Increment video counters
    vicii->vc = (vicii->vc + 1) & 0x3FF;  // 10-bit counter
    vicii->vmli = (vicii->vmli + 1) & 0x3F; // 6-bit counter
}

// Emit border pixels
void vicii_common_emit_border_pixels(vicii_common_t* vicii) {
    // Emit 8 border pixels (one character width)
    for (int i = 0; i < 8; i++) {
        if (vicii->pixel_line_index < vicii->visible_pixels_per_line) {
            vicii->pixel_line_priority[vicii->pixel_line_index] = vicii->border_pixel.priority;
            vicii->pixel_line_color[vicii->pixel_line_index] = vicii->border_pixel.color;
            vicii->pixel_line_index++;
        }
    }
}

// Emit graphics pixels based on graphics data
void vicii_common_emit_graphics_pixels(vicii_common_t* vicii, uint8_t graphics_data) {
    // Check if we're in multicolor mode
    bool multicolor = false;
    
    switch (vicii->graphics_mode) {
        case VICII_GM_MULTICOLOR_TEXT: {
            uint8_t color_code = vicii->video_color_line[vicii->vmli];
            multicolor = (color_code >> 3) & 1;
            break;
        }
        case VICII_GM_MULTICOLOR_BITMAP:
            multicolor = true;
            break;
    }
    
    if (multicolor) {
        // Multicolor mode: 4 double-width pixels, 2 bits per pixel
        for (int i = 0; i < 4; i++) {
            uint8_t color_index = (graphics_data >> (6 - i * 2)) & 0x03;
            vicii_pixel_t pixel = vicii->colors[color_index];
            
            // Emit double-width pixel
            for (int j = 0; j < 2; j++) {
                if (vicii->pixel_line_index < vicii->visible_pixels_per_line) {
                    vicii->pixel_line_priority[vicii->pixel_line_index] = pixel.priority;
                    vicii->pixel_line_color[vicii->pixel_line_index] = pixel.color;
                    vicii->pixel_line_index++;
                }
            }
        }
    } else {
        // Standard mode: 8 single-width pixels, 1 bit per pixel
        for (int i = 0; i < 8; i++) {
            uint8_t bit = (graphics_data >> (7 - i)) & 1;
            vicii_pixel_t pixel = vicii->colors[bit ? 4 : 0];  // Foreground or background
            
            if (vicii->pixel_line_index < vicii->visible_pixels_per_line) {
                vicii->pixel_line_priority[vicii->pixel_line_index] = pixel.priority;
                vicii->pixel_line_color[vicii->pixel_line_index] = pixel.color;
                vicii->pixel_line_index++;
            }
        }
    }
}

// Main cycle function with character and graphics access
void vicii_common_cycle(vicii_common_t* vicii) {
    // Count X position
    vicii->x_coordinate += 8;
    vicii->x_cycle++;
    
    // Handle horizontal retrace
    if (vicii->x_cycle >= vicii->cycles_per_line) {
        // Flush current pixel line to framebuffer before advancing to next line
        // Always flush if we have a framebuffer, even for raster lines beyond VIC-II's normal range
        if (vicii->framebuffer && vicii->raster_counter < vicii->framebuffer_height) {
            vicii_common_flush_pixel_line_to_output(vicii,
                vicii_common_get_default_palette(), vicii->raster_counter);
        }
        
        vicii->x_cycle = 0;
        vicii->x_coordinate = 0;
        vicii->pixel_line_index = 0;  // Reset pixel line index
        vicii->raster_counter++;
        
        // Handle vertical retrace - but continue generating lines until framebuffer is full
        if (vicii->raster_counter >= vicii->total_lines) {
            // If we haven't filled the entire framebuffer height, continue generating border lines
            if (vicii->framebuffer && vicii->raster_counter < vicii->framebuffer_height) {
                // Continue with border-only lines until framebuffer is complete
                // Don't reset VIC-II state, just keep generating border pixels
            } else {
                // Normal VIC-II vertical retrace
                vicii->raster_counter = 0;
                vicii->frame_count++;
                vicii->was_den_set_during_raster_30 = false;
                vicii->bad_line = false;
                vicii->vc_base = 0;
                vicii->lp_edge_detected = false;
            }
        } else {
            update_bad_line(vicii);
            handle_raster_interrupt(vicii);
        }
    }
    
    // Handle bad line related state
    if (vicii->bad_line) {
        // Set BA low during bad line (cycles 12-54)
        if (vicii->bus && vicii->x_cycle >= 12 && vicii->x_cycle <= 54) {
            ((c64_bus_t*)vicii->bus)->control_lines &= ~BA_LINE;
        }
        vicii->video_logic_display_state = true;
    } else {
        if (vicii->bus) {
            ((c64_bus_t*)vicii->bus)->control_lines |= BA_LINE;
        }
    }
    
    // VIC-II pixel generation - emit pixels for all framebuffer lines
    // Generate pixels to properly fill the framebuffer with correct centering
    if (vicii->framebuffer && vicii->raster_counter < vicii->framebuffer_height) {
        // Calculate how many cycles we need to fill the framebuffer width
        int cycles_needed = (vicii->framebuffer_width + 7) / 8;  // Round up to cover full width
        
        // Generate pixels for the full width, starting from cycle 0 to ensure proper centering
        if (vicii->x_cycle < cycles_needed) {
            bool in_normal_vic_range = vicii->raster_counter < vicii->total_lines;
            bool in_display_area = in_normal_vic_range &&
                                  (vicii->x_cycle >= 15 && vicii->x_cycle <= 54) &&
                                  (vicii->raster_counter >= vicii->border_top &&
                                   vicii->raster_counter <= vicii->border_bottom);
            
            if (in_display_area) {
                // In display area - emit graphics/text pixels (even in idle state for background color)
                vicii_common_g_access(vicii);
            } else {
                // Outside display area, display disabled, or beyond normal VIC range - emit border pixels
                vicii_common_emit_border_pixels(vicii);
            }
        }
    }
    
    // Handle display logic control (separate from pixel generation)
    if (vicii->x_cycle == 14) {
        // Reset video counters at start of display window
        vicii->vc = vicii->vc_base;
        vicii->vmli = 0;
        if (vicii->bad_line) {
            vicii->rc = 0;
        }
    }
    
    // Character access during bad lines (cycles 15-54)
    if (vicii->x_cycle >= 15 && vicii->x_cycle <= 54) {
        if (vicii->video_logic_display_state && vicii->bad_line) {
            vicii_common_c_access(vicii);
        }
    }
    
    // Handle end of character row
    if (vicii->x_cycle == 58) {
        if (vicii->rc == 7) {
            if (!vicii->bad_line) {
                vicii->video_logic_display_state = false;
                vicii->vc_base = vicii->vc;
                vicii->rc = 0;
            }
        }
        
        if (vicii->video_logic_display_state) {
            vicii->rc = (vicii->rc + 1) & 0x07;
        }
    }
}

// Macro to create RGBA color values for OpenGL GL_RGBA format
#define RGBA_COLOR(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

// C64 color palette (16 colors) in RGBA format for OpenGL
static const uint32_t c64_palette[16] = {
    RGBA_COLOR(0x00, 0x00, 0x00, 0xFF),  // 0: Black
    RGBA_COLOR(0xFF, 0xFF, 0xFF, 0xFF),  // 1: White
    RGBA_COLOR(0x68, 0x37, 0x2B, 0xFF),  // 2: Red
    RGBA_COLOR(0x70, 0xA4, 0xB2, 0xFF),  // 3: Cyan
    RGBA_COLOR(0x6F, 0x3D, 0x86, 0xFF),  // 4: Purple/Violet
    RGBA_COLOR(0x58, 0x8D, 0x43, 0xFF),  // 5: Green
    RGBA_COLOR(0x35, 0x28, 0x79, 0xFF),  // 6: Blue
    RGBA_COLOR(0xB8, 0xC7, 0x6F, 0xFF),  // 7: Yellow
    RGBA_COLOR(0x6F, 0x4F, 0x25, 0xFF),  // 8: Orange
    RGBA_COLOR(0x43, 0x39, 0x00, 0xFF),  // 9: Brown
    RGBA_COLOR(0x9A, 0x67, 0x59, 0xFF),  // 10: Light Red
    RGBA_COLOR(0x44, 0x44, 0x44, 0xFF),  // 11: Dark Grey
    RGBA_COLOR(0x6C, 0x6C, 0x6C, 0xFF),  // 12: Grey
    RGBA_COLOR(0x9A, 0xD2, 0x84, 0xFF),  // 13: Light Green
    RGBA_COLOR(0x6C, 0x5E, 0xB5, 0xFF),  // 14: Light Blue
    RGBA_COLOR(0x95, 0x95, 0x95, 0xFF)   // 15: Light Grey
};

// Get default C64 color palette
uint32_t* vicii_common_get_default_palette(void) {
    return (uint32_t*)c64_palette;
}

// Set framebuffer for VIC-II output
void vicii_common_set_framebuffer(vicii_common_t* vicii, uint32_t* framebuffer, int width, int height) {
    vicii->framebuffer = framebuffer;
    vicii->framebuffer_width = width;
    vicii->framebuffer_height = height;
    
    printf("VIC-II: Connected to framebuffer (%dx%d)\n", width, height);
    
    // Initialize VIC-II state to generate proper colors, but don't pre-fill framebuffer
    // Let the VIC-II cycle function generate the actual content
    vicii->border_pixel.priority = VICII_PRIORITY_BORDER;
    vicii->border_pixel.color = VICII_COLOR_LIGHT_BLUE;
    
    // Initialize pixel line buffers with proper border colors
    if (vicii->pixel_line_color && vicii->visible_pixels_per_line > 0) {
        for (int i = 0; i < vicii->visible_pixels_per_line; i++) {
            vicii->pixel_line_priority[i] = VICII_PRIORITY_BORDER;
            vicii->pixel_line_color[i] = VICII_COLOR_LIGHT_BLUE;
        }
    }
}

// Flush pixel line to framebuffer (equivalent to C# FlushPixelLineToOutput)
void vicii_common_flush_pixel_line_to_output(vicii_common_t* vicii, uint32_t* palette, int y) {
    if (!vicii->framebuffer || !palette || y >= vicii->framebuffer_height) {
        return;
    }
    
    // Calculate row address in framebuffer
    int row_address = y * vicii->framebuffer_width;
    
    // Always use current border color from register (this ensures proper color updates)
    uint8_t border_color_index = vicii->registers[VICII_EC] & 0x0F;
    uint32_t border_color = palette[border_color_index];
    
    // First, fill the entire line with border color
    for (int x = 0; x < vicii->framebuffer_width; x++) {
        vicii->framebuffer[row_address + x] = border_color;
    }
    
    // Then, overwrite with VIC-II generated pixels if any, centered in the framebuffer
    int pixels_to_copy = (vicii->pixel_line_index < vicii->visible_pixels_per_line) ?
                        vicii->pixel_line_index : vicii->visible_pixels_per_line;
    
    if (vicii->pixel_line_color && pixels_to_copy > 0) {
        // Center VIC-II visible area within the framebuffer
        int offset_x = (vicii->framebuffer_width - vicii->visible_pixels_per_line) / 2;
        
        for (int x = 0; x < pixels_to_copy; x++) {
            int fb_x = offset_x + x;
            if (fb_x >= 0 && fb_x < vicii->framebuffer_width) {
                uint8_t color_index = vicii->pixel_line_color[x] & 0x0F;  // Ensure 4-bit color
                vicii->framebuffer[row_address + fb_x] = palette[color_index];
            }
        }
    }
    
    // Reset pixel line index for next line
    vicii->pixel_line_index = 0;
}