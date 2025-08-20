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

// ROM memory read function - bus state interface
bus_state_t rom_memory_read(void* context, bus_state_t bus_state) {
    rom_t* rom = (rom_t*)context;
    bus_state.data = rom->memory[bus_state.addr & 0xFFFF];
    return bus_state;
}

chip_descriptor_t rom_descriptor = {
    .description = "ROM Chip (Read-Only Memory)",
    .create = rom_system_create,
    .destroy = rom_system_destroy,
    .bus_attach = NULL,
    .bank_change = NULL
};
