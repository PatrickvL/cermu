#include "rom.h"
#include <string.h>
#include <stdlib.h>

rom_t* rom_create() {
    rom_t* rom = (rom_t*)calloc(1, sizeof(rom_t));
    if (!rom) return NULL;

    return rom;
}

// Specialized ROM creation function that takes size parameter
rom_t* rom_create_with_size(unsigned int size) {
    rom_t* rom = rom_create();
    if (!rom) return NULL;
    // Note: memory pointer will be set later to point into unified buffer
    // No allocation needed here anymore
    rom->memory = NULL;
    rom->owns_memory = false;  // Memory will be owned by unified buffer
    return rom;
}

void rom_destroy(rom_t* rom) {
    if (!rom) return;
    // Only free memory if we own it (not pointing into unified buffer)
    if (rom->owns_memory && rom->memory) {
        free(rom->memory);
    }
    free(rom);
}

// ROM memory read function - bus state interface
bus_state_t rom_memory_read(void* context, bus_state_t bus_state) {
    rom_t* rom = (rom_t*)context;
    BUS_SET_DATA(bus_state, rom->memory[BUS_GET_ADDR(bus_state) & 0xFFFF]);
    return bus_state;
}
