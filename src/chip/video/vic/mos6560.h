#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/bus_cycle_interface.h"
#include "vic_common.h"

// VIC-6560 chip structure (inherits from vic_base_t via C++ inheritance)
typedef struct mos6560_s : public vic_base_t {
    // VIC-6560 specific fields (none currently, all in base)
} mos6560_t;

// Function prototypes
mos6560_t* mos6560_create();
void mos6560_destroy(mos6560_t* vic);
bus_state_t mos6560_tick(void* chip, bus_state_t bus_state);
void mos6560_bus_attach(void* chip, void* bus);
void mos6560_set_framebuffer(mos6560_t* vic, uint32_t* framebuffer, int width, int height);
void mos6560_reset(mos6560_t* vic);

// Register access functions
bus_state_t mos6560_registers_read(void* context, bus_state_t bus_state);
bus_state_t mos6560_registers_write(void* context, bus_state_t bus_state);