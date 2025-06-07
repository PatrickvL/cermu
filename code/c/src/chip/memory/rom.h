#ifndef ROM_H
#define ROM_H

#include "../../core/device.h"

typedef struct rom_s {
    device_descriptor_t* desc;
    uint8_t* memory;
    uint16_t size;
    uint16_t base_address;
} rom_t;

// Function declarations
void rom_memory_init(void* device, uint16_t base_address, uint16_t size, device_entry_t* device_entry);

#endif // ROM_H