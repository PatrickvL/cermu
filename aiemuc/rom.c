#include "rom.h"
#include "bus.h"
#include <string.h>

// ROM initialization
void rom_init(rom_state_t* rom_dev) {
    // Initialize ROM state
    memset(rom_dev, 0, sizeof(*rom_dev));
    
    // Set up device callbacks
    rom_dev->device.r8 = basic_r8;  // Default to basic, will be overridden by address mapping
    rom_dev->device.w8 = NULL;      // ROM is read-only
}

// ROM I/O handlers - called directly via callback table (no chip select checks!)
uint8_t basic_r8(struct device_s* dev) {
    rom_state_t* rom_dev = (rom_state_t*)dev;
    return rom_dev->basic_data[bus.address - 0xA000];
}

uint8_t char_rom_r8(struct device_s* dev) {
    rom_state_t* rom_dev = (rom_state_t*)dev;
    return rom_dev->char_data[bus.address - 0xD000];
}

uint8_t kernel_r8(struct device_s* dev) {
    rom_state_t* rom_dev = (rom_state_t*)dev;
    return rom_dev->kernal_data[bus.address - 0xE000];
}