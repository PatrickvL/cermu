#include "rom.h"
#include <string.h>
#include <stdlib.h>

void* rom_system_create(chip_descriptor_t* desc) {
    rom_t* rom = (rom_t*)calloc(1, sizeof(rom_t));
    if (!rom) return NULL;
    rom->desc = desc;
    return rom;
}

// Specialized ROM creation function that takes size parameter
void* rom_system_create_with_size(chip_descriptor_t* desc, unsigned int size) {
    rom_t* rom = (rom_t*)rom_system_create(desc);
    if (!rom) return NULL;
    // Allocate only the required size
    rom->memory = (uint8_t*)malloc(size);
    if (!rom->memory) {
        free(rom);
        return NULL;
    }
    return rom;
}

// Callback to provide rwcb_context for system registration
void* rom_get_rwcb_context(void* chip) {
    rom_t* rom = (rom_t*)chip;
    // ROM callback should return NULL if memory allocation failed during create
    return rom->memory; // Will be NULL if malloc failed in rom_system_create
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

chip_descriptor_t rom_descriptor = {
    .description = "ROM Chip (Read-Only Memory)",
    .create = rom_system_create,
    .destroy = rom_system_destroy,
    .bus_attach = NULL,
    .read = rom_memory_read, // ROM read receives pre-adjusted pointer vis rbcb_context
    .write = NULL, // ROM is read-only, no write function
    .bank_change = NULL,
    .get_rwcb_context = rom_get_rwcb_context
};
