#include "vic_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// VIC color palette (16 colors) - Hardware accurate VIC-20 colors
// Must match the palette defined in vic20_system.cpp for correct color rendering
// Format: 0xAARRGGBB (Alpha=0xFF, Red, Green, Blue) - Same format as C64 palette
static const uint32_t vic_palette[16] = {
    0xFF000000, // 0: Black
    0xFFFFFFFF, // 1: White
    0xFF813338, // 2: Red
    0xFF75CEC8, // 3: Cyan
    0xFF8E3C97, // 4: Purple/Magenta
    0xFF56AC4D, // 5: Green
    0xFF2B338D, // 6: Blue
    0xFFEDF171, // 7: Yellow
    0xFFC46C71, // 8: Orange/Brown
    0xFFFFD4A1, // 9: Light Orange/Tan
    0xFF9A6759, // 10: Light Red/Pink
    0xFFC7FFFF, // 11: Light Cyan (Colodore standard)
    0xFFC9ADFF, // 12: Light Purple/Lavender
    0xFF9AE29B, // 13: Light Green
    0xFF7873C4, // 14: Light Blue
    0xFFFFFFB0  // 15: Light Yellow
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

    // Default register values for VIC-20 PAL (hardware power-on defaults)
    // Based on VIC-I (6560/6561) hardware specifications
    vic->registers[VIC_REG_CONTROL1] = 0x0C;  // $9000: Horizontal centering (PAL: $0C, NTSC: $05)
    vic->registers[VIC_REG_CONTROL2] = 0x26;  // $9001: Vertical centering (38 rows)
    vic->registers[VIC_REG_VIDEO_MATRIX] = 0x96;  // $9002: Columns: 22, Video matrix at $1000
    vic->registers[VIC_REG_ROWS] = 0x2E;  // $9003: Rows: 23 (×2 = 46 rows), char size 8×16
    vic->registers[VIC_REG_RASTER] = 0x00;  // $9004: TV raster value (read-only)
    vic->registers[VIC_REG_CHAR_BASE] = 0xF0;  // $9005: Character memory at $1000, screen origin
    vic->registers[VIC_REG_LIGHTPEN_X] = 0x00;  // $9006: Light pen horizontal
    vic->registers[VIC_REG_LIGHTPEN_Y] = 0x00;  // $9007: Light pen vertical
    vic->registers[VIC_REG_PADDLE_X] = 0x00;  // $9008: Paddle X
    vic->registers[VIC_REG_PADDLE_Y] = 0x00;  // $9009: Paddle Y
    vic->registers[VIC_REG_OSC1_FREQ] = 0x00; // $900A: Bass switch/frequency
    vic->registers[VIC_REG_OSC2_FREQ] = 0x00; // $900B: Alto frequency
    vic->registers[VIC_REG_OSC3_FREQ] = 0x00; // $900C: Soprano frequency
    vic->registers[VIC_REG_OSC4_FREQ] = 0x00; // $900D: Noise frequency
    vic->registers[VIC_REG_AUX_COLOR] = 0x00; // $900E: Auxiliary color, volume = 0 (muted)
    vic->registers[VIC_REG_BACKGROUND] = 0x1B; // $900F: Screen colors: Border=Cyan(3), BG=White(1), Reverse=ON(0)

    // Reset video generation state
    vic->in_display_area = false;
    vic->in_char_area = false;
    vic->matrix_index = 0;
    vic->matrix_video_byte = 0;
    vic->matrix_color_byte = 0;
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
    // Emit 4 pixels per cycle
    if (vic->in_display_area && vic->in_char_area) {
        // Extract frequently accessed register values
        const uint8_t reg_char_base = vic->registers[VIC_REG_CHAR_BASE];
        const uint8_t reg_background = vic->registers[VIC_REG_BACKGROUND];
        
        // Extract base addresses and colors (used for pixel rendering)
        const uint16_t base_video = ((vic->registers[VIC_REG_VIDEO_MATRIX] & VIC_VM_BASE_VIDEO_BIT9) << 2) |
                                   ((reg_char_base & VIC_CB_BASE_VIDEO_MASK) << VIC_CB_BASE_VIDEO_SHIFT);
        const uint16_t base_char = (reg_char_base & VIC_CB_BASE_CHAR_MASK) << VIC_CB_BASE_CHAR_SHIFT;
        // Derive is_char_fetch_cycle from cycle position: even cycles relative to screen_origin_x are fetch cycles
        const bool is_char_fetch_cycle = ((vic->current_cycle - screen_origin_x) & 1) == 0;
        
        // TODO: Handle double height
        if (is_char_fetch_cycle) {
            // Fetch character data from memory
            if (vic->mem_read && vic->color_read) {
                vic->matrix_video_byte = vic->mem_read(vic->mem_user_data, base_video | (vic->matrix_index / 8));
                vic->matrix_color_byte = vic->color_read(vic->color_user_data, vic->matrix_index / 8);
                
                // Calculate character ROM address and fetch: base_char | (video_byte << 3) | (raster_line & 7) | 0x8000
                vic->matrix_char_data = vic->mem_read(vic->mem_user_data,
                    (base_char | ((uint16_t)vic->matrix_video_byte << 3) | (vic->raster_counter & 7)) ^ 0x8000);
            } else {
                // No memory access available - emit blank
                vic->matrix_char_data = 0;
                vic->matrix_color_byte = 0;
            }
            
            vic->matrix_index++;
        }
        else {
            // Each second character cycle, emit the lower nyble
            vic->matrix_char_data <<= 4;
        }

        const uint8_t foreground_color = vic->matrix_color_byte & VIC_COLOR_FOREGROUND_MASK;
        const uint8_t background_color = (reg_background & VIC_BG_BACKGROUND_MASK) >> VIC_BG_BACKGROUND_SHIFT;

        // Emit high nyble (4 pixels)
        if (vic->matrix_color_byte & VIC_COLOR_MULTICOLOR) {  // Multicolor mode
            const uint8_t border_color = reg_background & VIC_BG_BORDER_MASK;
            const uint8_t auxiliary_color = (vic->registers[VIC_REG_AUX_COLOR] & VIC_AUX_COLOR_MASK) >> VIC_AUX_COLOR_SHIFT;
            // Emit 2 pixels for bits 7-6
            uint8_t color = 0;
            switch ((vic->matrix_char_data >> 6) & 3) {
                case 0b00: color = background_color; break;
                case 0b01: color = border_color; break;
                case 0b10: color = foreground_color; break;
                case 0b11: color = auxiliary_color; break;
            }
            vic_emit_pixel(vic, color);
            vic_emit_pixel(vic, color);

            // Emit 2 pixels for bits 5-4
            switch ((vic->matrix_char_data >> 4) & 3) {
                case 0b00: color = background_color; break;
                case 0b01: color = border_color; break;
                case 0b10: color = foreground_color; break;
                case 0b11: color = auxiliary_color; break;
            }
            vic_emit_pixel(vic, color);
            vic_emit_pixel(vic, color);
        }
        else {  // Hires mode
            const bool reversed = (reg_background & VIC_BG_REVERSED) != 0;
            const uint8_t char_data = reversed ? ~vic->matrix_char_data : vic->matrix_char_data;
            vic_emit_pixel(vic, (char_data & 0x80) ? foreground_color : background_color);
            vic_emit_pixel(vic, (char_data & 0x40) ? foreground_color : background_color);
            vic_emit_pixel(vic, (char_data & 0x20) ? foreground_color : background_color);
            vic_emit_pixel(vic, (char_data & 0x10) ? foreground_color : background_color);       
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
