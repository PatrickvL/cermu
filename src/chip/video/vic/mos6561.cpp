#include "mos6561.h"
#include "vic_common.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// MOS6561 chip descriptor
chip_descriptor_t mos6561_descriptor = {
    .description = "MOS6561 VIC Video Interface Controller (Enhanced)",
    .create = [](chip_descriptor_t*) -> void* { return mos6561_create(); },
    .destroy = [](void* chip) { mos6561_destroy(static_cast<mos6561_t*>(chip)); },
    .bus_attach = (void (*)(void *, void *))mos6561_bus_attach
};

// MOS6561 chip configuration — PAL variant
// The MOS 6561 is the PAL version of the VIC-I chip used in PAL VIC-20s.
// PAL crystal: 4.433619 MHz / 4 = 1,108,405 Hz system clock
// 63 cycles per line, 312 total lines per frame → ~50 Hz refresh
// NOTE: VICE uses 71 cycles/line for PAL; 63 here matches the existing
// working VIC rendering code and should be reviewed separately.
static const vic_chip_config_t vic_config_pal = {
    .cycles_per_line = VIC_PAL_CYCLES_PER_LINE,
    .total_lines = VIC_PAL_TOTAL_LINES,
    .clock_frequency = 1108405,
    .chip_name = "MOS6561 PAL",
    .is_pal = true
};

mos6561_t* mos6561_create() {
    mos6561_t* vic = new mos6561_t();

    vic->is_pal = true;
    vic->clock_frequency = vic_config_pal.clock_frequency;
    vic->config = &vic_config_pal;

    // Initialize registers
    memset(vic->registers, 0, sizeof(vic->registers));
    memset(vic->color_ram, 0, sizeof(vic->color_ram));

    // Default timing for PAL
    vic->cycles_per_line = VIC_PAL_CYCLES_PER_LINE;
    vic->total_lines = VIC_PAL_TOTAL_LINES;

    // Enable enhanced features
    vic->extended_color_mode = true;
    vic->extended_colors[0] = 0x00; // Black
    vic->extended_colors[1] = 0xFF; // White
    vic->extended_colors[2] = 0x88; // Gray 1
    vic->extended_colors[3] = 0xAA; // Gray 2

    // Reset video generation state
    vic_system_reset(vic);

    // Initialise audio with PAL clock and default sample rate
    vic_audio_reset(vic, vic_config_pal.clock_frequency, 22050);

    return vic;
}

void mos6561_destroy(mos6561_t* vic) {
    if (!vic) return;
    delete vic;
}

void mos6561_bus_attach(void* chip, void* bus) {
    vic_bus_attach(chip, bus);
}

void mos6561_set_framebuffer(mos6561_t* vic, uint32_t* framebuffer, int width, int height) {
    vic_set_framebuffer(vic, framebuffer, width, height);
}

void mos6561_reset(mos6561_t* vic) {
    vic_system_reset(vic);

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

    return vic_read_register(vic, reg);
}

void mos6561_write_register(mos6561_t* vic, uint8_t reg, uint8_t value) {
    if (!vic || reg >= 16) return;

    // Handle extended color registers
    if (vic->extended_color_mode && reg >= 12 && reg <= 15) {
        vic->extended_colors[reg - 12] = value;
        return;
    }

    vic_write_register(vic, reg, value);
}

// Main tick function
bus_state_t mos6561_tick(void* chip, bus_state_t bus_state) {
    return vic_tick(chip, bus_state);
}