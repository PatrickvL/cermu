#include "rom.h"
#include <string.h>
#include <stdlib.h>

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

static void rom_cycle(struct device_s* dev) { (void)dev; } // ROM has no cycle logic

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
    .cycle = rom_cycle,
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