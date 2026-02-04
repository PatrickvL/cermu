#include "vic_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// VIC color palette (16 colors) - VIC-20 specific colors
static const uint32_t vic_palette[16] = {
    0xFF000000, 0xFFFFFFFF, 0xFF68372B, 0xFF70A4B2,
    0xFF6F3D86, 0xFF588D43, 0xFF352879, 0xFFB8C76F,
    0xFF6F4F25, 0xFF433900, 0xFF9A6759, 0xFF444444,
    0xFF6C6C6C, 0xFF9AD284, 0xFF6C5EB5, 0xFF959595
};

// Get default VIC palette
uint32_t* vic_get_default_palette(void) {
    return (uint32_t*)vic_palette;
}

// Set memory callbacks for VIC
void vic_set_memory_callbacks(vic_base_t* vic, vic_mem_read_fn_t mem_read, void* mem_user_data,
                              vic_mem_read_fn_t color_read, void* color_user_data) {
    if (!vic) return;
    vic->mem_read = mem_read;
    vic->mem_user_data = mem_user_data;
    vic->color_read = color_read;
    vic->color_user_data = color_user_data;
}

// System reset function
void vic_system_reset(vic_base_t* vic) {
    if (!vic) return;

    // Reset registers to default values matching C# initialization
    memset(vic->registers, 0, sizeof(vic->registers));
    vic->raster_counter = 0;
    vic->current_cycle = 0;

    // Default register values from C# Initialize()
    vic->registers[VIC_REG_CONTROL1] = 0x0C;  // Control1: ScreenOriginX = 12
    vic->registers[VIC_REG_CONTROL2] = 0x26;  // Control2: ScreenOriginY = 38 << 1 = 76
    vic->registers[VIC_REG_VIDEO_MATRIX] = 0x16;  // Video matrix columns
    vic->registers[VIC_REG_ROWS] = 0x2E;  // Video matrix rows
    vic->registers[VIC_REG_RASTER] = 0x00;  // Raster value
    vic->registers[VIC_REG_CHAR_BASE] = 0xF0;  // Character base = 240
    vic->registers[VIC_REG_LIGHTPEN_X] = 0x00;  // Light pen X
    vic->registers[VIC_REG_LIGHTPEN_Y] = 0x00;  // Light pen Y
    vic->registers[VIC_REG_PADDLE_X] = 0xFF;  // Paddle X
    vic->registers[VIC_REG_PADDLE_Y] = 0xFF;  // Paddle Y
    vic->registers[VIC_REG_OSC1_FREQ] = 0x00; // Oscillator 1
    vic->registers[VIC_REG_OSC2_FREQ] = 0x00; // Oscillator 2
    vic->registers[VIC_REG_OSC3_FREQ] = 0x00; // Oscillator 3
    vic->registers[VIC_REG_OSC4_FREQ] = 0x00; // Oscillator 4
    vic->registers[VIC_REG_AUX_COLOR] = 0x00; // Auxiliary color + Volume
    vic->registers[VIC_REG_BACKGROUND] = 0x1B; // Background + Border = 27

    // Reset video generation state
    vic->in_display_area = false;
    vic->in_char_area = false;
    vic->matrix_index = 0;
    vic->matrix_video_byte = 0;
    vic->foreground_color = 0;
    vic->matrix_char_data = 0;
    vic->pixel_line_index = 0;
    
    // Initialize memory callbacks to NULL (system must set them)
    vic->mem_read = NULL;
    vic->mem_user_data = NULL;
    vic->color_read = NULL;
    vic->color_user_data = NULL;
}

// Bus attach function
void vic_bus_attach(void* chip, void* bus) {
    vic_base_t* vic = (vic_base_t*)chip;
    if (!vic) return;
    vic->bus = bus;
}

// Set framebuffer function
void vic_set_framebuffer(vic_base_t* vic, uint32_t* framebuffer, int width, int height) {
    if (!vic) return;
    vic->framebuffer = framebuffer;
    vic->framebuffer_width = width;
    vic->framebuffer_height = height;
}

// Register read function
uint8_t vic_read_register(vic_base_t* vic, uint8_t reg) {
    if (!vic || reg >= 16) return 0;

    // Handle special registers that require computed values from tick state
    switch (reg) {
        case VIC_REG_ROWS: // RasterLine bit 0 | NoOfVideoMatrixRows | DoubleHeight
            return ((vic->raster_counter & 1) << 7) | (vic->registers[reg] & 0x7F);
        case VIC_REG_RASTER: // RasterLine bits 8-1
            return (uint8_t)(vic->raster_counter >> 1);
        default:
            return vic->registers[reg];
    }
}

// Register write function
void vic_write_register(vic_base_t* vic, uint8_t reg, uint8_t value) {
    if (!vic || reg >= 16) return;
    vic->registers[reg] = value;
}

// Emit a single pixel to the line buffer
static inline void vic_emit_pixel(vic_base_t* vic, uint8_t color_index) {
    if (!vic) return;
    if (vic->pixel_line_index < 284) {  // Max line width
        vic->pixel_line_buffer[vic->pixel_line_index++] = vic_palette[color_index & 0x0F];
    }
}

// Flush accumulated pixel line to framebuffer
static void vic_flush_pixel_line(vic_base_t* vic, int raster_line) {
    if (!vic || !vic->framebuffer) return;
    if (raster_line < 0 || raster_line >= vic->framebuffer_height) return;

    // Copy pixel line buffer to framebuffer
    int pixels_to_copy = vic->pixel_line_index;
    if (pixels_to_copy > vic->framebuffer_width) {
        pixels_to_copy = vic->framebuffer_width;
    }

    uint32_t* dest = vic->framebuffer + (raster_line * vic->framebuffer_width);
    memcpy(dest, vic->pixel_line_buffer, pixels_to_copy * sizeof(uint32_t));
    
    // Fill remaining pixels with border color if line is shorter
    if (pixels_to_copy < vic->framebuffer_width) {
        uint8_t border_color = vic->registers[VIC_REG_BACKGROUND] & VIC_BG_BORDER_MASK;
        uint32_t border_pixel = vic_palette[border_color];
        for (int i = pixels_to_copy; i < vic->framebuffer_width; i++) {
            dest[i] = border_pixel;
        }
    }

    // Reset pixel line index for next line
    vic->pixel_line_index = 0;
}

// Main tick function (main video generation) - based on C# ClockCycle()
// Main tick function (main video generation) - based on C# ClockCycle()
bus_state_t vic_tick(void* chip, bus_state_t bus_state) {
    vic_base_t* vic = (vic_base_t*)chip;
    if (!vic) return bus_state;

    // Increment cycle counter
    vic->current_cycle++;
    if (vic->current_cycle >= vic->cycles_per_line) {
        vic->current_cycle = 0;
        
        // Flush the previous line to framebuffer
        vic_flush_pixel_line(vic, vic->raster_counter);
        
        // Move to next raster line
        vic->raster_counter++;
        if (vic->raster_counter >= vic->total_lines) {
            vic->raster_counter = 0;
        }

        // Check if entering/leaving display area
        const uint16_t screen_origin_y = vic->registers[VIC_REG_CONTROL2] << 1;
        if (vic->raster_counter == screen_origin_y) {
            vic->in_display_area = true;
            vic->matrix_index = 0;
        }
        else if (vic->raster_counter == screen_origin_y + (((vic->registers[VIC_REG_ROWS] & VIC_ROWS_ROWS_MASK) >> VIC_ROWS_ROWS_SHIFT) << 3)) {
            vic->in_display_area = false;
        }
    }

    // Check character area boundaries
    const uint16_t screen_origin_x = vic->registers[VIC_REG_CONTROL1] & VIC_C1_SCREEN_ORIGIN_X_MASK;
    if (vic->current_cycle == screen_origin_x) {
        vic->in_char_area = true;
    }
    else if (vic->current_cycle == screen_origin_x + ((vic->registers[VIC_REG_VIDEO_MATRIX] & VIC_VM_COLUMNS_MASK) << 1)) {
        vic->in_char_area = false;
    }

    // Emit 4 pixels per cycle
    // Emit 4 pixels per cycle
    if (vic->in_display_area && vic->in_char_area) {
        // Extract frequently accessed register values
        const uint8_t reg_char_base = vic->registers[VIC_REG_CHAR_BASE];
        const uint8_t reg_background = vic->registers[VIC_REG_BACKGROUND];
        
        // Extract base addresses and colors (used for pixel rendering)
        const uint16_t base_video = ((vic->registers[VIC_REG_VIDEO_MATRIX] & VIC_VM_BASE_VIDEO_BIT9) << 2) |
                                   ((reg_char_base & VIC_CB_BASE_VIDEO_MASK) << VIC_CB_BASE_VIDEO_SHIFT);
        const uint16_t base_char = (reg_char_base & VIC_CB_BASE_CHAR_MASK) << VIC_CB_BASE_CHAR_SHIFT;
        const uint8_t auxiliary_color = (vic->registers[VIC_REG_AUX_COLOR] & VIC_AUX_COLOR_MASK) >> VIC_AUX_COLOR_SHIFT;
        const uint8_t background_color = (reg_background & VIC_BG_BACKGROUND_MASK) >> VIC_BG_BACKGROUND_SHIFT;
        const uint8_t border_color = reg_background & VIC_BG_BORDER_MASK;
        const bool reversed = (reg_background & VIC_BG_REVERSED) != 0;
        
        // Derive is_char_fetch_cycle from cycle position: even cycles relative to screen_origin_x are fetch cycles
        const bool is_char_fetch_cycle = ((vic->current_cycle - screen_origin_x) & 1) == 0;
        
        // TODO: Handle double height
        if (is_char_fetch_cycle) {
            // Fetch character data from memory
            if (vic->mem_read && vic->color_read) {
                vic->matrix_video_byte = vic->mem_read(vic->mem_user_data, base_video | (vic->matrix_index / 8));
                vic->foreground_color = vic->color_read(vic->color_user_data, vic->matrix_index / 8);
                
                // Calculate character ROM address and fetch: base_char | (video_byte << 3) | (raster_line & 7) | 0x8000
                vic->matrix_char_data = vic->mem_read(vic->mem_user_data,
                    (base_char | ((uint16_t)vic->matrix_video_byte << 3) | (vic->raster_counter & 7)) ^ 0x8000);
            } else {
                // No memory access available - emit blank
                vic->matrix_char_data = 0;
                vic->foreground_color = 0;
            }
            
            vic->matrix_index++;
        }
        else {
            // Each second character cycle, emit the lower nyble
            vic->matrix_char_data <<= 4;
        }

        // Emit high nyble (4 pixels)
        if (vic->foreground_color & VIC_COLOR_MULTICOLOR) {  // Multicolor mode
            // Emit 2 pixels for bits 7-6
            uint8_t color = 0;
            switch ((vic->matrix_char_data >> 6) & 3) {
                case 0b00: color = background_color; break;
                case 0b01: color = border_color; break;
                case 0b10: color = vic->foreground_color & VIC_COLOR_FOREGROUND_MASK; break;
                case 0b11: color = auxiliary_color; break;
            }
            vic_emit_pixel(vic, color);
            vic_emit_pixel(vic, color);

            // Emit 2 pixels for bits 5-4
            switch ((vic->matrix_char_data >> 4) & 3) {
                case 0b00: color = background_color; break;
                case 0b01: color = border_color; break;
                case 0b10: color = vic->foreground_color & VIC_COLOR_FOREGROUND_MASK; break;
                case 0b11: color = auxiliary_color; break;
            }
            vic_emit_pixel(vic, color);
            vic_emit_pixel(vic, color);
        }
        else {  // Hires mode
            const uint8_t foreground = vic->foreground_color & VIC_COLOR_FOREGROUND_MASK;
            vic_emit_pixel(vic, (vic->matrix_char_data & 0x80) == (reversed ? 0 : 0x80) ? foreground : background_color);
            vic_emit_pixel(vic, (vic->matrix_char_data & 0x40) == (reversed ? 0 : 0x40) ? foreground : background_color);
            vic_emit_pixel(vic, (vic->matrix_char_data & 0x20) == (reversed ? 0 : 0x20) ? foreground : background_color);
            vic_emit_pixel(vic, (vic->matrix_char_data & 0x10) == (reversed ? 0 : 0x10) ? foreground : background_color);
        }
    }
    else {
        // Emit 4 border pixels
        const uint8_t border = vic->registers[VIC_REG_BACKGROUND] & VIC_BG_BORDER_MASK;
        vic_emit_pixel(vic, border);
        vic_emit_pixel(vic, border);
        vic_emit_pixel(vic, border);
        vic_emit_pixel(vic, border);
    }

    return bus_state;
}
