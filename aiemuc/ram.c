#include "ram.h"
#include "bus.h"

// 64K RAM
uint8_t ram[65536];

// RAM I/O handlers - called directly via callback table (no chip select checks!)
void ram_read_handler(void) {
    bus.data = ram[bus.address];
}

void ram_write_handler(void) {
    ram[bus.address] = bus.data;
}
