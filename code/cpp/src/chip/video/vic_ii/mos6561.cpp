#include "mos6561.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// MOS6561 chip descriptor
chip_descriptor_t mos6561_descriptor = {
    .description = "MOS6561 VIC Video Interface Controller (Enhanced)",
    .create = mos6561_create,
    .destroy = mos6561_destroy,
    .bus_attach = (void (*)(void *, void *))mos6561_bus_attach,
    .bank_change = NULL
};

void* mos6561_create(chip_descriptor_t* desc) {
    mos6561_t* vic = (mos6561_t*)calloc(1, sizeof(mos6561_t));
    if (!vic) return NULL;

    vic->desc = desc;
    vic->is_pal = true; // Default to PAL
    vic->clock_frequency = 886723; // PAL clock frequency

    // Initialize registers
    memset(vic->registers, 0, sizeof(vic->registers));
    memset(vic->color_ram, 0, sizeof(vic->color_ram));

    // Default timing for PAL
    vic->cycles_per_line = 63;
    vic->total_lines = 312;

    // Enable enhanced features
    vic->extended_color_mode = true;
    vic->extended_colors[0] = 0x00; // Black
    vic->extended_colors[1] = 0xFF; // White
    vic->extended_colors[2] = 0x88; // Gray 1
    vic->extended_colors[3] = 0xAA; // Gray 2

    return vic;
}

void mos6561_destroy(void* chip) {
    if (!chip) return;
    mos6561_t* vic = (mos6561_t*)chip;
    free(vic);
}

void mos6561_bus_attach(void* chip, void* bus) {
    if (!chip) return;
    mos6561_t* vic = (mos6561_t*)chip;
    vic->bus = bus;
}

void mos6561_reset(mos6561_t* vic) {
    if (!vic) return;

    // Reset registers to default values
    memset(vic->registers, 0, sizeof(vic->registers));
    vic->raster_counter = 0;
    vic->current_cycle = 0;

    // Default register values for MOS6561
    vic->registers[0] = 0x0F; // Control register
    vic->registers[1] = 0x00; // Raster counter
    vic->registers[2] = 0x00; // Light pen X
    vic->registers[3] = 0x00; // Light pen Y
    vic->registers[4] = 0x1B; // Enable register

    // Reset extended features
    vic->extended_color_mode = true;
}

void mos6561_set_framebuffer(mos6561_t* vic, uint32_t* framebuffer, int width, int height) {
    if (!vic) return;
    vic->framebuffer = framebuffer;
    vic->framebuffer_width = width;
    vic->framebuffer_height = height;
}

bus_state_t mos6561_tick(void* chip, bus_state_t bus_state) {
    mos6561_t* vic = (mos6561_t*)chip;
    if (!vic) return bus_state;

    // Advance raster counter
    vic->current_cycle++;
    if (vic->current_cycle >= vic->cycles_per_line) {
        vic->current_cycle = 0;
        vic->raster_counter++;

        if (vic->raster_counter >= vic->total_lines) {
            vic->raster_counter = 0;
        }
    }

    // Enhanced VIC-6561 emulation with extended color support
    // TODO: Implement full VIC-6561 video generation with extended colors

    return bus_state;
}

// Enhanced register access functions
uint8_t mos6561_read_register(mos6561_t* vic, uint8_t reg) {
    if (!vic || reg >= 16) return 0;

    // Handle extended color registers (if implemented)
    if (vic->extended_color_mode && reg >= 12 && reg <= 15) {
        // Return extended color information
        return vic->extended_colors[reg - 12];
    }

    return vic->registers[reg];
}

void mos6561_write_register(mos6561_t* vic, uint8_t reg, uint8_t value) {
    if (!vic || reg >= 16) return;

    // Handle extended color registers
    if (vic->extended_color_mode && reg >= 12 && reg <= 15) {
        vic->extended_colors[reg - 12] = value;
        return;
    }

    vic->registers[reg] = value;

    // Handle special registers
    switch (reg) {
        case 0x04: // Enable register
            vic->extended_color_mode = (value & 0x80) != 0;
            break;
        // Other registers handled normally
    }
}