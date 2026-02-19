#include "mos6561.h"
#include "vic_common.h"
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

// VIC-6561 chip configuration
static const vic_chip_config_t vic_config_pal = {
    .cycles_per_line = VIC_PAL_CYCLES_PER_LINE,
    .total_lines = VIC_PAL_TOTAL_LINES,
    .clock_frequency = 886723,
    .chip_name = "MOS6561 PAL",
    .is_pal = true
};

void* mos6561_create(chip_descriptor_t* desc) {
    mos6561_t* vic = (mos6561_t*)calloc(1, sizeof(mos6561_t));
    if (!vic) return NULL;

    vic->base.desc = desc;
    vic->base.is_pal = true;
    vic->base.clock_frequency = 886723; // PAL clock frequency
    vic->base.config = &vic_config_pal;

    // Initialize registers
    memset(vic->base.registers, 0, sizeof(vic->base.registers));
    memset(vic->base.color_ram, 0, sizeof(vic->base.color_ram));

    // Default timing for PAL
    vic->base.cycles_per_line = VIC_PAL_CYCLES_PER_LINE;
    vic->base.total_lines = VIC_PAL_TOTAL_LINES;

    // Enable enhanced features
    vic->extended_color_mode = true;
    vic->extended_colors[0] = 0x00; // Black
    vic->extended_colors[1] = 0xFF; // White
    vic->extended_colors[2] = 0x88; // Gray 1
    vic->extended_colors[3] = 0xAA; // Gray 2

    // Reset video generation state
    vic_system_reset(&vic->base);

    // Initialise audio with PAL clock and default sample rate
    vic_audio_reset(&vic->base, vic_config_pal.clock_frequency, 22050);

    return vic;
}

void mos6561_destroy(void* chip) {
    if (!chip) return;
    mos6561_t* vic = (mos6561_t*)chip;
    free(vic);
}

void mos6561_bus_attach(void* chip, void* bus) {
    vic_bus_attach(chip, bus);
}

void mos6561_set_framebuffer(mos6561_t* vic, uint32_t* framebuffer, int width, int height) {
    vic_set_framebuffer(&vic->base, framebuffer, width, height);
}

void mos6561_reset(mos6561_t* vic) {
    vic_system_reset(&vic->base);

    // Reset extended features
    vic->extended_color_mode = true;
}

// Enhanced register access functions
uint8_t mos6561_read_register(mos6561_t* vic, uint8_t reg) {
    if (!vic || reg >= 16) return 0;

    // Handle extended color registers (if implemented)
    if (vic->extended_color_mode && reg >= 12 && reg <= 15) {
        // Return extended color information
        return vic->extended_colors[reg - 12];
    }

    return vic_read_register(&vic->base, reg);
}

void mos6561_write_register(mos6561_t* vic, uint8_t reg, uint8_t value) {
    if (!vic || reg >= 16) return;

    // Handle extended color registers
    if (vic->extended_color_mode && reg >= 12 && reg <= 15) {
        vic->extended_colors[reg - 12] = value;
        return;
    }

    vic_write_register(&vic->base, reg, value);
}

// Main tick function
bus_state_t mos6561_tick(void* chip, bus_state_t bus_state) {
    return vic_tick(chip, bus_state);
}