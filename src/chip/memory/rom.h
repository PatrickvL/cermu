#pragma once

#include "../../core/system_lines.h"

typedef struct rom_s {
    uint8_t* memory = nullptr;
    bool owns_memory = false;

    ~rom_s();
} rom_t;

// ROM access functions - bus state interface
bus_state_t rom_memory_read(void* context, bus_state_t bus_state);

