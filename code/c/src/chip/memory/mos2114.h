#ifndef MOS2114_H
#define MOS2114_H

#include <stdint.h>
#include "../../core/system.h"

// MOS Technology 2114 Static RAM - 1K x 4-bit
// Used as ColorRAM in C64 at 0xD800-0xDBFF
// Only the lower 4 bits are used (color information)
// https://www.amiga-stuff.com/hardware/1kx4-sram.html
typedef struct mos2114_s {
    chip_descriptor_t* desc;
    uint8_t* memory;  // Pointer to memory (will point into unified buffer)
} mos2114_t;

// Chip descriptor
extern chip_descriptor_t mos2114_descriptor;

// Creation and destruction functions
void* mos2114_create(chip_descriptor_t* desc);
void mos2114_destroy(void* chip);

// Read/Write functions - bus state interface
bus_state_t mos2114_read(void* context, bus_state_t bus_state);
bus_state_t mos2114_write(void* context, bus_state_t bus_state);

#endif // MOS2114_H