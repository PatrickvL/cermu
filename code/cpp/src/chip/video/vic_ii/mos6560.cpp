#include "mos6560.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// VIC-6560 chip descriptor
chip_descriptor_t mos6560_descriptor = {
    .description = "MOS6560/6561 VIC Video Interface Controller",
    .create = mos6560_create,
    .destroy = mos6560_destroy,
    .bus_attach = (void (*)(void *, void *))mos6560_bus_attach,
    .bank_change = NULL
};

void* mos6560_create(chip_descriptor_t* desc) {
    mos6560_t* vic = (mos6560_t*)calloc(1, sizeof(mos6560_t));
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

    return vic;
}

void mos6560_destroy(void* chip) {
    if (!chip) return;
    mos6560_t* vic = (mos6560_t*)chip;
    free(vic);
}

void mos6560_bus_attach(void* chip, void* bus) {
    if (!chip) return;
    mos6560_t* vic = (mos6560_t*)chip;
    vic->bus = bus;
}

void mos6560_reset(mos6560_t* vic) {
    if (!vic) return;

    // Reset registers to default values
    memset(vic->registers, 0, sizeof(vic->registers));
    vic->raster_counter = 0;
    vic->current_cycle = 0;

    // Default register values
    vic->registers[0] = 0x0F; // Control register
    vic->registers[1] = 0x00; // Raster counter
    vic->registers[2] = 0x00; // Light pen X
    vic->registers[3] = 0x00; // Light pen Y
    vic->registers[4] = 0x1B; // Enable register
}

void mos6560_set_framebuffer(mos6560_t* vic, uint32_t* framebuffer, int width, int height) {
    if (!vic) return;
    vic->framebuffer = framebuffer;
    vic->framebuffer_width = width;
    vic->framebuffer_height = height;
}

bus_state_t mos6560_tick(void* chip, bus_state_t bus_state) {
    mos6560_t* vic = (mos6560_t*)chip;
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

    // Simple VIC-6560 emulation - just handle basic timing for now
    // TODO: Implement full VIC-6560 video generation

    return bus_state;
}