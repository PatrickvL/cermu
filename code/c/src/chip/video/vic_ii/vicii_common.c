#include "vicii_common.h"
#include "../../../systems/c64/c64_bus.h"
#include <stdlib.h>
#include <string.h>

// Helper function to read clear (collision registers)
static uint8_t read_clear(vicii_common_t* vicii, uint8_t reg) {
    uint8_t val = vicii->registers[reg];
    vicii->registers[reg] = 0;
    return val;
}

// Helper function to set main border flip flop
static void set_main_border_flip_flop(vicii_common_t* vicii, bool main_border_flip_flop) {
    if (main_border_flip_flop) {
        vicii->border_pixel.priority = VICII_PRIORITY_BORDER;
        vicii->border_pixel.color = vicii->registers[VICII_EC] & 0x0F;
    } else {
        vicii->border_pixel.priority = VICII_PRIORITY_BACKGROUND;
        vicii->border_pixel.color = vicii->registers[VICII_B0C] & 0x0F;
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
    vicii->bank = bank;
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
    set_main_border_flip_flop(vicii, false);
    
    // Allocate pixel buffers
    if (vicii->visible_pixels_per_line > 0) {
        vicii->pixel_line_priority = malloc(vicii->visible_pixels_per_line * sizeof(vicii_priority_t));
        vicii->pixel_line_color = malloc(vicii->visible_pixels_per_line * sizeof(uint32_t));
    }
}

// Main cycle function - simplified version of the complex cycle-by-cycle implementation
void vicii_common_cycle(vicii_common_t* vicii) {
    // Count X position
    vicii->x_coordinate += 8;
    vicii->x_cycle++;
    
    // Handle horizontal retrace
    if (vicii->x_cycle >= vicii->cycles_per_line) {
        vicii->x_cycle = 0;
        vicii->x_coordinate = 0;
        vicii->raster_counter++;
        
        // Handle vertical retrace
        if (vicii->raster_counter >= vicii->total_lines) {
            vicii->raster_counter = 0;
            vicii->frame_count++;
            vicii->was_den_set_during_raster_30 = false;
            vicii->bad_line = false;
            vicii->vc_base = 0;
            vicii->lp_edge_detected = false;
        } else {
            update_bad_line(vicii);
            handle_raster_interrupt(vicii);
        }
    }
    
    // Handle bad line related state
    if (vicii->bad_line) {
        // Set BA low during bad line
        if (vicii->bus) {
            ((c64_bus_t*)vicii->bus)->control_lines &= ~BA_LINE;
        }
        vicii->video_logic_display_state = true;
    } else {
        if (vicii->bus) {
            ((c64_bus_t*)vicii->bus)->control_lines |= BA_LINE;
        }
    }
    
    // Handle display logic
    if (vicii->x_cycle >= 14 && vicii->x_cycle <= 54) {
        // In display window
        if (vicii->x_cycle == 14) {
            vicii->vc = vicii->vc_base;
            vicii->vmli = 0;
            if (vicii->bad_line) {
                vicii->rc = 0;
            }
        }
        
        // Character and graphics access would happen here
        // (Simplified for now - full implementation would do c_access and g_access)
    }
    
    // Handle sprite logic (simplified)
    // Full sprite implementation would handle all 8 sprites with proper timing
    
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