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

// System reset function
void vic_system_reset(vic_base_t* vic) {
    if (!vic) return;

    // Reset registers to default values
    memset(vic->registers, 0, sizeof(vic->registers));
    vic->raster_counter = 0;
    vic->current_cycle = 0;

    // Default register values
    vic->registers[VIC_REG_CONTROL1] = 0x0F; // Control register
    vic->registers[VIC_REG_CONTROL2] = 0x00; // Raster counter
    vic->registers[VIC_REG_LIGHTPEN_X] = 0x00; // Light pen X
    vic->registers[VIC_REG_LIGHTPEN_Y] = 0x00; // Light pen Y
    vic->registers[VIC_REG_ENABLE] = 0x1B; // Enable register

    // Reset video generation state
    vic->in_display_area = false;
    vic->in_char_area = false;
    vic->is_char_fetch_cycle = false;
    vic->matrix_index = 0;
    vic->matrix_video_byte = 0;
    vic->foreground_color = 0;
    vic->matrix_char_data = 0;
    vic->screen_origin_x = 0;
    vic->screen_origin_y = 0;
    vic->no_of_columns = 22; // Default 22 columns
    vic->no_of_rows = 23;   // Default 23 rows
    vic->double_height = false;
    vic->base_video = 0;
    vic->base_char = 0;
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

    // Handle special registers
    switch (reg) {
        case VIC_REG_RASTER:
            return (uint8_t)(vic->raster_counter & 0xFF);
        case VIC_REG_LIGHTPEN_X:
            return vic->registers[reg]; // TODO: Implement light pen
        case VIC_REG_LIGHTPEN_Y:
            return vic->registers[reg]; // TODO: Implement light pen
        default:
            return vic->registers[reg];
    }
}

// Register write function
void vic_write_register(vic_base_t* vic, uint8_t reg, uint8_t value) {
    if (!vic || reg >= 16) return;

    // Handle special registers
    switch (reg) {
        case VIC_REG_CONTROL1:
            vic->screen_origin_x = value & VIC_C1_SCREEN_ORIGIN_X;
            vic->in_display_area = (value & VIC_C1_INTERLACE) != 0;
            break;
        case VIC_REG_CONTROL2:
            vic->screen_origin_y = value & VIC_C2_SCREEN_ORIGIN_Y;
            break;
        case VIC_REG_VIDEO_MATRIX:
            vic->base_video = (value << 6) & 0x3FC0;
            break;
        case VIC_REG_CHAR_BASE:
            vic->base_char = (value << 10) & 0x3C00;
            break;
        case VIC_REG_ENABLE:
            // Handle oscillator enables
            break;
        default:
            vic->registers[reg] = value;
    }
}

// Clock cycle function (main video generation)
void vic_clock_cycle(vic_base_t* vic) {
    if (!vic) return;

    vic->current_cycle++;
    if (vic->current_cycle >= vic->cycles_per_line) {
        vic->current_cycle = 0;
        vic->raster_counter++;

        if (vic->raster_counter >= vic->total_lines) {
            vic->raster_counter = 0;
        }

        // Check display area boundaries
        if (vic->raster_counter == vic->screen_origin_y) {
            vic->in_display_area = true;
            vic->matrix_index = 0;
        }
        else if (vic->raster_counter == vic->screen_origin_y + (vic->no_of_rows * 8)) {
            vic->in_display_area = false;
        }
    }

    // Check character area boundaries
    if (vic->current_cycle == vic->screen_origin_x) {
        vic->in_char_area = true;
        vic->is_char_fetch_cycle = true;
    }
    else if (vic->current_cycle == vic->screen_origin_x + (vic->no_of_columns * 2)) {
        vic->in_char_area = false;
    }

    // Character fetching and pixel emission
    if (vic->in_display_area && vic->in_char_area) {
        if (vic->is_char_fetch_cycle) {
            // Fetch character data
            vic->matrix_video_byte = 0; // TODO: Read from memory
            vic->foreground_color = 0;  // TODO: Read from color RAM
            vic->matrix_char_data = 0;  // TODO: Read character data
            vic->matrix_index++;
            vic->is_char_fetch_cycle = false;
        }
        else {
            // Process character data
            vic->matrix_char_data <<= 4;
            vic->is_char_fetch_cycle = true;
        }

        // Emit pixels (simplified)
        vic_emit_pixel(vic, vic->foreground_color);
    }
    else {
        // Emit border pixels
        vic_emit_pixel(vic, vic->registers[VIC_REG_BORDER_COLOR]);
    }
}

// Pixel emission function
void vic_emit_pixel(vic_base_t* vic, uint8_t color) {
    if (!vic || !vic->framebuffer) return;

    // Simple pixel emission - would be enhanced with proper color mapping
    uint32_t pixel_color = 0xFF000000 | (color * 0x111111); // Simple grayscale for now

    int x = vic->current_cycle;
    int y = vic->raster_counter;

    if (x < vic->framebuffer_width && y < vic->framebuffer_height) {
        vic->framebuffer[y * vic->framebuffer_width + x] = pixel_color;
    }
}

// Flush pixel line function
void vic_flush_pixel_line(vic_base_t* vic) {
    if (!vic || !vic->framebuffer) return;

    // Flush current pixel line to framebuffer
    // This would be enhanced with proper line rendering
}

// Main tick function
bus_state_t vic_tick(void* chip, bus_state_t bus_state) {
    vic_base_t* vic = (vic_base_t*)chip;
    if (!vic) return bus_state;

    // Call clock cycle for video generation
    vic_clock_cycle(vic);

    // Update timing
    vic->current_cycle++;
    if (vic->current_cycle >= vic->cycles_per_line) {
        vic->current_cycle = 0;
        vic->raster_counter++;

        if (vic->raster_counter >= vic->total_lines) {
            vic->raster_counter = 0;
        }
    }

    return bus_state;
}