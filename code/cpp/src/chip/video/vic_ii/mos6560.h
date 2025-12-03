#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/bus_cycle_interface.h"
#include "../../core/chip.h"

// VIC-6560/6561 chip structure
typedef struct {
    chip_descriptor_t* desc;
    void* bus;

    // Registers
    uint8_t registers[16];

    // Video state
    uint16_t raster_counter;
    uint8_t current_line[40]; // 40 characters per line
    uint8_t color_ram[1024]; // 1KB color RAM

    // Timing
    uint32_t cycles_per_line;
    uint32_t total_lines;
    uint32_t current_cycle;

    // Framebuffer
    uint32_t* framebuffer;
    int framebuffer_width;
    int framebuffer_height;

    // Configuration
    bool is_pal;
    uint32_t clock_frequency;
} mos6560_t;

// Chip descriptor
extern chip_descriptor_t mos6560_descriptor;

// Function prototypes
void* mos6560_create(chip_descriptor_t* desc);
void mos6560_destroy(void* chip);
bus_state_t mos6560_tick(void* chip, bus_state_t bus_state);
void mos6560_bus_attach(void* chip, void* bus);
void mos6560_set_framebuffer(mos6560_t* vic, uint32_t* framebuffer, int width, int height);
void mos6560_reset(mos6560_t* vic);