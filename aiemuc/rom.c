#include "rom.h"
#include "bus.h"
#include <string.h>

// ROM device instance
rom_state_t rom;

// ROM initialization
void rom_init(void) {
    // Initialize ROM state
    memset(&rom, 0, sizeof(rom));
    
    // Set up device callbacks
    rom.device.r8 = basic_r8;  // Default to basic, will be overridden by address mapping
    rom.device.w8 = NULL;      // ROM is read-only
}

// ROM I/O handlers - called directly via callback table (no chip select checks!)
uint8_t basic_r8(void) {
    return rom.basic_data[bus.address - 0xA000];
}

uint8_t char_rom_r8(void) {
    return rom.char_data[bus.address - 0xD000];
}

uint8_t kernel_r8(void) {
    return rom.kernal_data[bus.address - 0xE000];
}