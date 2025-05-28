#include "rom.h"
#include "bus.h"

// ROM images
uint8_t basic_rom[8192];
uint8_t char_rom[4096];
uint8_t kernal_rom[8192];

// ROM I/O handlers - called directly via callback table (no chip select checks!)
void basic_read_handler(void) {
    bus.data = basic_rom[bus.address - 0xA000];
}

void char_rom_read_handler(void) {
    bus.data = char_rom[bus.address - 0xD000];
}

void kernel_read_handler(void) {
    bus.data = kernal_rom[bus.address - 0xE000];
}