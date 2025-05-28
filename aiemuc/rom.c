#include "rom.h"
#include "bus.h"

// ROM images
uint8_t kernal_rom[8192];
uint8_t basic_rom[8192];
uint8_t char_rom[4096];

// ROM I/O handlers - called directly via callback table (no chip select checks!)
void rom_read_handler(void) {
    if (bus_state.address >= 0xE000) {
        bus_state.data = kernal_rom[bus_state.address - 0xE000];
    } else {
        bus_state.data = basic_rom[bus_state.address - 0xA000];
    }
}

// Character ROM I/O handlers
void char_rom_read_handler(void) {
    bus_state.data = char_rom[bus_state.address - 0xD000];
}
