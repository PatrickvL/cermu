#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/bus_cycle_interface.h"
#include "vic_common.h"

// VIC-6561 chip structure (inherits from vic_base_t via C++ inheritance)
typedef struct mos6561_s : public vic_base_t {
    // VIC-6561 specific fields
    bool extended_color_mode = false;
    uint8_t extended_colors[4] = {};
} mos6561_t;

// Function prototypes
mos6561_t* mos6561_create();
void mos6561_destroy(mos6561_t* vic);
bus_state_t mos6561_tick(void* chip, bus_state_t bus_state);
void mos6561_bus_attach(void* chip, void* bus);
void mos6561_set_framebuffer(mos6561_t* vic, uint32_t* framebuffer, int width, int height);
void mos6561_reset(mos6561_t* vic);

// Enhanced register access functions
bus_state_t mos6561_registers_read(void* context, bus_state_t bus_state);
bus_state_t mos6561_registers_write(void* context, bus_state_t bus_state);