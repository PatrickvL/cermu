#include "ram.h"
#include "bus.h"

// 64K RAM
uint8_t ram[65536];

// RAM I/O handlers - called directly via callback table (no chip select checks!)
void ram_read_handler(void) {
    bus_state.data = ram[bus_state.address];
}

void ram_write_handler(void) {
    // Handle the CPU port address writes
    if (bus_state.address == 0x001) {
        uint8_t direction = ram[0x0000]; // Data Direction Register (DDR at $0000). 1 = set, 0 = read&clear
        uint8_t io_mask = ram[0x0001]; // I/O Port Data (at $0001)

        io_mask &= ~direction; // clear the mask bits that will be overwritten
        io_mask |= direction & bus_state.data; // set the appropriate bits from value
        extern void switch_cpu_mode(uint8_t mode);
        switch_cpu_mode(io_mask); // Apply the new mode to the PLA
        ram[bus_state.address] = io_mask; // Write the adjusted value to I/O Port Data (at $0001)
    } else {
        // Normal RAM writes
        ram[bus_state.address] = bus_state.data;
    }
}
