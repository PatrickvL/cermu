#include "rom.h"
#include "bus.h"
#include <string.h>
#include <stdlib.h>

// Generic ROM read handler
uint8_t rom_r8(struct device_s* dev) {
    rom_state_t* rom_dev = (rom_state_t*)dev;
    uint16_t offset = bus.address - rom_dev->base_address;
    
    // Bounds check
    if (offset >= rom_dev->size) {
        return 0xFF;  // Return default value for out-of-bounds access
    }
    
    return rom_dev->data[offset];
}

// ROM cleanup - free allocated memory
void rom_cleanup(rom_state_t* rom_dev) {
    if (rom_dev && rom_dev->data) {
        free(rom_dev->data);
        rom_dev->data = NULL;
        rom_dev->size = 0;
    }
}

// Generic ROM initialization with configurable size and base address
void rom_init(rom_state_t* rom_dev, uint16_t size, uint16_t base_address) {
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
    
    // Set up device callbacks
    rom_dev->device.r8 = rom_r8;
    rom_dev->device.w8 = NULL;  // ROM is read-only
}