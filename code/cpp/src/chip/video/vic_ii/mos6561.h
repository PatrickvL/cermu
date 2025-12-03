#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/bus_cycle_interface.h"
#include "../../core/chip.h"

// VIC-6561 chip structure (enhanced version of 6560)
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

    // Enhanced features
    bool extended_color_mode;
    uint8_t extended_colors[4];
} mos6561_t;

// Chip descriptor
extern chip_descriptor_t mos6561_descriptor;

// Function prototypes
void* mos6561_create(chip_descriptor_t* desc);
void mos6561_destroy(void* chip);
bus_state_t mos6561_tick(void* chip, bus_state_t bus_state);
void mos6561_bus_attach(void* chip, void* bus);
void mos6561_set_framebuffer(mos6561_t* vic, uint32_t* framebuffer, int width, int height);
void mos6561_reset(mos6561_t* vic);