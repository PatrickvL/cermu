#ifndef MOS2114_H
#define MOS2114_H

#include <stdint.h>
#include "../../core/system.h"

// MOS Technology 2114 Static RAM - 1K x 4-bit
// Used as ColorRAM in C64 at 0xD800-0xDBFF
// Only the lower 4 bits are used (color information)
// https://www.amiga-stuff.com/hardware/1kx4-sram.html
typedef struct mos2114_s {
    device_descriptor_t* desc;
    uint8_t memory[1024];  // 1K x 4-bit (stored as bytes, only lower 4 bits used)
} mos2114_t;

// Device descriptor
extern device_descriptor_t mos2114_descriptor;

// Creation and destruction functions
void* mos2114_create(device_descriptor_t* desc);
void mos2114_destroy(void* device);

// Bus attach function to set up rwcb_context
void mos2114_memory_init(void* device, device_entry_t* device_entry);

// Read/Write functions
uint8_t mos2114_read(void* context, uint16_t address);
void mos2114_write(void* context, uint16_t address, uint8_t value);

#endif // MOS2114_H