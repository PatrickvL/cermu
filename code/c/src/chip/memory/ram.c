#include "ram.h"
#include <string.h>
#include <stdlib.h>

void* ram_system_create(chip_descriptor_t* desc) {
    ram_t* ram = (ram_t*)calloc(1, sizeof(ram_t));
    if (!ram) return NULL;
    ram->desc = desc;
    return ram;
}

void ram_system_destroy(void* context) {
    free(context);
}

// Callback to provide rwcb_context for system registration
void* ram_get_rwcb_context(void* chip) {
    ram_t* ram = (ram_t*)chip;
    return ram->memory;
}

uint8_t ram_memory_read(void* context, uint16_t address) {
    uint8_t* memory = (uint8_t*)context;
    return memory[address];
}

void ram_memory_write(void* context, uint16_t address, uint8_t value) {
    uint8_t* memory = (uint8_t*)context;
    memory[address] = value;
}

chip_descriptor_t ram_descriptor = {
    .create = ram_system_create,
    .destroy = ram_system_destroy,
    .bus_attach = NULL,
    .read = ram_memory_read,
    .write = ram_memory_write,
    .bank_change = NULL,
    .get_rwcb_context = ram_get_rwcb_context
};
