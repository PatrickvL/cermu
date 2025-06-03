#ifndef RAM_H
#define RAM_H

#include "device.h"

typedef struct ram_s {
    device_descriptor_t* desc;
    uint8_t memory[65536];
} ram_t;

// Function declarations
void ram_memory_init(void* device, device_entry_t* device_entry);

#endif // RAM_H