#include "rom.h"
#include <string.h>
#include <stdlib.h>

void* rom_system_create(device_descriptor_t* desc) {
    rom_t* rom = (rom_t*)calloc(1, sizeof(rom_t));
    if (!rom) return NULL;
    rom->desc = desc;
    // Memory allocation and addressing will be done in rom_memory_init
    rom->memory = NULL;
    rom->size = 0;
    rom->base_address = 0;
    return rom;
}

// Initialize ROM with device entry information
void rom_memory_init(void* device, uint16_t base_address, uint16_t size, device_entry_t* device_entry) {
    rom_t* rom = (rom_t*)device;
    if (!rom) return;
    
    rom->size = size;
    rom->base_address = base_address;
    rom->memory = (uint8_t*)malloc(rom->size);
    
    // Set pre-adjusted rwcb_context so ROM can reuse RAM read code
    if (device_entry && rom->memory) {
        device_entry->rwcb_context = rom->memory - base_address;
    }
    
    // Note: Memory content will be loaded by c64_memory_init
}

void rom_system_destroy(void* context) {
    rom_t* rom = (rom_t*)context;
    if (!rom) return;
    free(rom->memory);
    free(rom);
}

uint8_t rom_memory_read(void* context, uint16_t address) {
    uint8_t* memory = (uint8_t*)context;
    return memory[address];
}

device_descriptor_t rom_descriptor = {
    .create = rom_system_create,
    .destroy = rom_system_destroy,
    .bus_attach = NULL,
    .read = rom_memory_read, // ROM read receives pre-adjusted pointer vis rbcb_context
    .write = NULL, // ROM is read-only, no write function
    .bank_change = NULL
};
