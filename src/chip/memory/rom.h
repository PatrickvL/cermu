#pragma once

#include "../../core/chip.h"

typedef struct rom_s {
    chip_descriptor_t* desc;
    uint8_t* memory;
    bool owns_memory;  // True if this chip owns the memory and should free it on destruction
} rom_t;

// ROM access functions - bus state interface
bus_state_t rom_memory_read(void* context, bus_state_t bus_state);

// Typed lifecycle functions
rom_t* rom_create();
rom_t* rom_create_with_size(unsigned int size);
void rom_destroy(rom_t* rom);

extern chip_descriptor_t rom_descriptor;

