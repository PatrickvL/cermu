#ifndef RAM_H
#define RAM_H

#include "device.h"

typedef struct ram_s {
    device_descriptor_t* desc;
    uint8_t memory[65536];
} ram_t;

// Function declarations
void ram_memory_init(void* device, device_entry_t* device_entry);

// Direct RAM access functions
uint8_t ram_memory_read(void* context, uint16_t address);
void ram_memory_write(void* context, uint16_t address, uint8_t value);

#endif // RAM_H