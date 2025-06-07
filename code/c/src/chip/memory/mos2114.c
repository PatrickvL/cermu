#include "mos2114.h"
#include <stdlib.h>
#include <string.h>

void* mos2114_create(device_descriptor_t* desc) {
    mos2114_t* mos2114 = (mos2114_t*)calloc(1, sizeof(mos2114_t));
    if (!mos2114) return NULL;
    mos2114->desc = desc;
    // Initialize all memory to 0 (which is also the default color)
    memset(mos2114->memory, 0, sizeof(mos2114->memory));
    return mos2114;
}

void mos2114_destroy(void* device) {
    free(device);
}

// Initialize MOS2114 with device entry information
void mos2114_memory_init(void* device, device_entry_t* device_entry) {
    mos2114_t* mos2114 = (mos2114_t*)device;
    if (!mos2114 || !device_entry) return;
    
    // Set rwcb_context to memory buffer for direct memory access
    device_entry->rwcb_context = mos2114->memory;
}

uint8_t mos2114_read(void* context, uint16_t address) {
    uint8_t* memory = (uint8_t*)context;
    // MOS2114 has 1024 addresses, mask to 10 bits
    return memory[address & 0x3FF];
}

void mos2114_write(void* context, uint16_t address, uint8_t value) {
    uint8_t* memory = (uint8_t*)context;
    // MOS2114 is 4-bit wide, so only store lower 4 bits
    // Address mask to 10 bits (1024 addresses)
    memory[address & 0x3FF] = value & 0x0F;
}

device_descriptor_t mos2114_descriptor = {
    .create = mos2114_create,
    .destroy = mos2114_destroy,
    .bus_attach = NULL,
    .read = mos2114_read,
    .write = mos2114_write,
    .bank_change = NULL
};