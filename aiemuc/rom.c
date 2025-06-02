#include "rom.h"
#include <string.h>
#include <stdlib.h>

void* rom_system_create(device_descriptor_t* desc) {
    rom_t* rom = (rom_t*)calloc(1, sizeof(rom_t));
    if (!rom) return NULL;
    rom->desc = desc;
    for (int i = 0; i < c64->system.device_count; i++) {
        if (c64->system.devices[i].device == rom) {
            rom->size = c64->system.devices[i].size;
            rom->base_address = c64->system.devices[i].base_address;
            rom->memory = (uint8_t*)malloc(rom->size);
            if (!rom->memory) {
                free(rom);
                return NULL;
            }
            break;
        }
    }
    return rom;
}

void rom_system_destroy(void* context) {
    rom_t* rom = (rom_t*)context;
    free(rom->memory);
    free(rom);
}

uint8_t rom_memory_read(void* context, uint16_t address) {
    uint8_t* memory = (uint8_t*)context;
    rom_t* rom = NULL;
    for (int i = 0; i < c64->system.device_count; i++) {
        if (c64->system.devices[i].desc == &rom_descriptor &&
            address >= c64->system.devices[i].base_address &&
            address < c64->system.devices[i].base_address + c64->system.devices[i].size) {
            rom = (rom_t*)c64->system.devices[i].device;
            break;
        }
    }
    if (rom) {
        return memory[address - rom->base_address];
    }
    return 0;
}

void rom_memory_write(void* context, uint16_t address, uint8_t value) {
    // ROM is read-only
}

static device_descriptor_t rom_descriptor = {
    .create = rom_system_create,
    .destroy = rom_system_destroy,
    .read = rom_memory_read,
    .write = rom_memory_write,
    .bank_change = NULL
};
