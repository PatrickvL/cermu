#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/bus_cycle_interface.h"
#include "../../core/chip.h"
#include "vic_common.h"

// VIC-6561 chip structure (inherits from base)
typedef struct {
    vic_base_t base; // Base VIC structure

    // VIC-6561 specific fields
    bool extended_color_mode;
    uint8_t extended_colors[4];
} mos6561_t;

// Chip descriptor
extern chip_descriptor_t mos6561_descriptor;

// Function prototypes
mos6561_t* mos6561_create();
void mos6561_destroy(mos6561_t* vic);
bus_state_t mos6561_tick(void* chip, bus_state_t bus_state);
void mos6561_bus_attach(void* chip, void* bus);
void mos6561_set_framebuffer(mos6561_t* vic, uint32_t* framebuffer, int width, int height);
void mos6561_reset(mos6561_t* vic);

// Enhanced register access functions
uint8_t mos6561_read_register(mos6561_t* vic, uint8_t reg);
void mos6561_write_register(mos6561_t* vic, uint8_t reg, uint8_t value);