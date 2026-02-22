#include "rom.h"
#include <stdlib.h>

rom_s::~rom_s() {
    if (owns_memory && memory) {
        free(memory);
    }
}

// ROM memory read function - bus state interface
bus_state_t rom_memory_read(void* context, bus_state_t bus_state) {
    rom_t* rom = (rom_t*)context;
    BUS_SET_DATA(bus_state, rom->memory[BUS_GET_ADDR(bus_state) & 0xFFFF]);
    return bus_state;
}
