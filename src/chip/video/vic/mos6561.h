#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "vic_common.h"

// VIC-6561 chip structure (inherits from vic_base_t via C++ inheritance)
typedef struct mos6561_s : public vic_base_t {
    // VIC-6561 specific fields
    bool extended_color_mode = false;
    uint8_t extended_colors[4] = {};

    void init();  // Initialize PAL-specific configuration
    void reset() override;
    bus_state_t registers_read(bus_state_t bus_state) override;
    bus_state_t registers_write(bus_state_t bus_state) override;
} mos6561_t;