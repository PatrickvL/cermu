#include "rom.h"
#include <string.h>
#include <stdlib.h>

void* rom_system_create(void* bus) {
    rom_t* rom = (rom_t*)calloc(1, sizeof(rom_t));
    if (!rom) return NULL;
    rom->desc = &rom_descriptor;
    rom->bus = (bus_interface_t*)bus;
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

// old
// Generic ROM read handler
uint8_t rom_r8(struct device_s* dev, uint16_t address) {
    rom_state_t* rom_dev = (rom_state_t*)dev;
    uint16_t offset = address - rom_dev->base_address;
    
    // Bounds check
    if (offset >= rom_dev->size) {
        return 0xFF;  // Return default value for out-of-bounds access
    }
    
    return rom_dev->data[offset];
}
// Forward declaration of device descriptor
static const device_t rom_device_descriptor;

// Direct implementation functions for device lifecycle
static void rom_init(struct device_s* dev) {
    // ROM init is a no-op since setup happens via rom_setup()
    (void)dev;
}

static void rom_cleanup(struct device_s* dev) {
    rom_state_t* rom_dev = (rom_state_t*)dev;
    if (rom_dev && rom_dev->data) {
        free(rom_dev->data);
        rom_dev->data = NULL;
        rom_dev->size = 0;
    }
}

// Static device descriptor for ROM
static const device_t rom_device_descriptor = {
    .r8 = rom_r8,
    .w8 = NULL,  // ROM is read-only
    .init = rom_init,
    .cycle = NULL,
    .cleanup = rom_cleanup
};

// ROM setup with configurable size and base address (call before device_init)
void rom_setup(rom_state_t* rom_dev, uint16_t size, uint16_t base_address) {
    // Initialize ROM state
    memset(rom_dev, 0, sizeof(*rom_dev));
    
    // Allocate memory for ROM data
    rom_dev->data = malloc(size);
    if (rom_dev->data) {
        memset(rom_dev->data, 0, size);
    }
    
    // Set ROM parameters
    rom_dev->size = size;
    rom_dev->base_address = base_address;
    
    // Set up device callbacks from descriptor pointer
    rom_dev->device = &rom_device_descriptor;
}