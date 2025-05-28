#include "ram.h"
#include "bus.h"
#include <string.h>

// RAM device instance
ram_state_t ram;

// RAM initialization
void ram_init(void) {
    // Initialize RAM state
    memset(&ram, 0, sizeof(ram));
    
    // Set up device callbacks
    ram.device.r8 = ram_r8;
    ram.device.w8 = ram_w8;
}

// RAM I/O handlers - called directly via callback table (no chip select checks!)
uint8_t ram_r8(struct device_s* dev) {
    ram_state_t* ram_dev = (ram_state_t*)dev;
    return ram_dev->data[bus.address];
}

void ram_w8(struct device_s* dev) {
    ram_state_t* ram_dev = (ram_state_t*)dev;
    ram_dev->data[bus.address] = bus.data;
}
