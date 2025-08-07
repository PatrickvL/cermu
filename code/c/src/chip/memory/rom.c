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
    // Note: memory pointer will be set later to point into unified buffer
    // No allocation needed here anymore
    rom->memory = NULL;
    return rom;
}

void rom_system_destroy(void* context) {
    rom_t* rom = (rom_t*)context;
    if (!rom) return;
    // Don't free memory pointer since it points into unified buffer
    free(rom);
}

uint8_t rom_memory_read(void* chip, uint16_t address) {
    rom_t* rom = (rom_t*)chip;
    return rom->memory[address];
}

chip_descriptor_t rom_descriptor = {
    .description = "ROM Chip (Read-Only Memory)",
    .create = rom_system_create,
    .destroy = rom_system_destroy,
    .bus_attach = NULL,
    .read = rom_memory_read, // ROM read receives chip pointer directly
    .write = NULL, // ROM is read-only, no write function
    .bank_change = NULL
};
