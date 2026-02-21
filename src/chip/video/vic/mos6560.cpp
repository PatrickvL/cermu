#include "mos6560.h"
#include "vic_common.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// VIC-6560 chip descriptor
chip_descriptor_t mos6560_descriptor = {
    .description = "MOS6560/6561 VIC Video Interface Controller",
    .create = [](chip_descriptor_t*) -> void* { return mos6560_create(); },
    .destroy = [](void* chip) { mos6560_destroy(static_cast<mos6560_t*>(chip)); },
    .bus_attach = (void (*)(void *, void *))mos6560_bus_attach,
    .bank_change = NULL
};

// VIC-6560 chip configuration — NTSC variant
// The MOS 6560 is the NTSC version of the VIC-I chip used in NTSC VIC-20s.
// NTSC crystal: 14.31818 MHz / 14 = 1,022,727 Hz system clock
// 65 cycles per line, 262 total lines per frame → ~60 Hz refresh
static const vic_chip_config_t vic_config_ntsc = {
    .cycles_per_line = VIC_NTSC_CYCLES_PER_LINE,
    .total_lines = VIC_NTSC_TOTAL_LINES,
    .clock_frequency = 1022727,
    .chip_name = "MOS6560 NTSC",
    .is_pal = false
};

mos6560_t* mos6560_create() {
    mos6560_t* vic = (mos6560_t*)calloc(1, sizeof(mos6560_t));
    if (!vic) return NULL;

    vic->base.is_pal = false;
    vic->base.clock_frequency = vic_config_ntsc.clock_frequency;
    vic->base.config = &vic_config_ntsc;

    // Initialize registers
    memset(vic->base.registers, 0, sizeof(vic->base.registers));
    memset(vic->base.color_ram, 0, sizeof(vic->base.color_ram));

    // Default timing for NTSC
    vic->base.cycles_per_line = VIC_NTSC_CYCLES_PER_LINE;
    vic->base.total_lines = VIC_NTSC_TOTAL_LINES;

    // Reset video generation state
    vic_system_reset(&vic->base);

    // Initialise audio with NTSC clock and default sample rate
    vic_audio_reset(&vic->base, vic_config_ntsc.clock_frequency, 22050);

    return vic;
}

void mos6560_destroy(mos6560_t* vic) {
    if (!vic) return;
    free(vic);
}

void mos6560_bus_attach(void* chip, void* bus) {
    vic_bus_attach(chip, bus);
}

void mos6560_set_framebuffer(mos6560_t* vic, uint32_t* framebuffer, int width, int height) {
    vic_set_framebuffer(&vic->base, framebuffer, width, height);
}

void mos6560_reset(mos6560_t* vic) {
    vic_system_reset(&vic->base);
}

// Register access functions
uint8_t mos6560_read_register(mos6560_t* vic, uint8_t reg) {
    return vic_read_register(&vic->base, reg);
}

void mos6560_write_register(mos6560_t* vic, uint8_t reg, uint8_t value) {
    vic_write_register(&vic->base, reg, value);
}

// Main tick function
bus_state_t mos6560_tick(void* chip, bus_state_t bus_state) {
    return vic_tick(chip, bus_state);
}