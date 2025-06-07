#include "ram.h"
#include "c64.h"
#include <string.h>
#include <stdlib.h>

void* ram_system_create(device_descriptor_t* desc) {
    ram_t* ram = (ram_t*)calloc(1, sizeof(ram_t));
    if (!ram) return NULL;
    ram->desc = desc;
    return ram;
}

void ram_system_destroy(void* context) {
    free(context);
}

// Initialize RAM with device entry information
void ram_memory_init(void* device, device_entry_t* device_entry) {
    ram_t* ram = (ram_t*)device;
    if (!ram || !device_entry) return;
    
    // Set rwcb_context to memory buffer for direct memory access
    device_entry->rwcb_context = ram->memory;
}

uint8_t ram_memory_read(void* context, uint16_t address) {
    uint8_t* memory = (uint8_t*)context;
    return memory[address];
}

void ram_memory_write(void* context, uint16_t address, uint8_t value) {
    uint8_t* memory = (uint8_t*)context;
    memory[address] = value;
}

device_descriptor_t ram_descriptor = {
    .create = ram_system_create,
    .destroy = ram_system_destroy,
    .bus_attach = NULL,
    .read = ram_memory_read,
    .write = ram_memory_write,
    .bank_change = NULL
};
