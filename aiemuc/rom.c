#include "rom.h"
#include "bus.h"
#include <string.h>

// Basic ROM initialization
void basic_rom_init(basic_rom_state_t* basic_dev) {
    // Initialize Basic ROM state
    memset(basic_dev, 0, sizeof(*basic_dev));
    
    // Set up device callbacks
    basic_dev->device.r8 = basic_rom_r8;
    basic_dev->device.w8 = NULL;  // ROM is read-only
}

// Kernal ROM initialization
void kernal_rom_init(kernal_rom_state_t* kernal_dev) {
    // Initialize Kernal ROM state
    memset(kernal_dev, 0, sizeof(*kernal_dev));
    
    // Set up device callbacks
    kernal_dev->device.r8 = kernal_rom_r8;
    kernal_dev->device.w8 = NULL;  // ROM is read-only
}

// Character ROM initialization
void char_rom_init(char_rom_state_t* char_dev) {
    // Initialize Character ROM state
    memset(char_dev, 0, sizeof(*char_dev));
    
    // Set up device callbacks
    char_dev->device.r8 = char_rom_r8;
    char_dev->device.w8 = NULL;  // ROM is read-only
}

// ROM I/O handlers - called directly via callback table (no chip select checks!)
uint8_t basic_rom_r8(struct device_s* dev) {
    basic_rom_state_t* basic_dev = (basic_rom_state_t*)dev;
    return basic_dev->data[bus.address - 0xA000];
}

uint8_t char_rom_r8(struct device_s* dev) {
    char_rom_state_t* char_dev = (char_rom_state_t*)dev;
    return char_dev->data[bus.address - 0xD000];
}

uint8_t kernal_rom_r8(struct device_s* dev) {
    kernal_rom_state_t* kernal_dev = (kernal_rom_state_t*)dev;
    return kernal_dev->data[bus.address - 0xE000];
}