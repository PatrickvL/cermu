#include "ram.h"
#include "bus.h"
#include <string.h>

// Forward declaration of descriptor
static const device_t ram_device_descriptor;

// RAM I/O handlers - called directly via callback table (no chip select checks!)
uint8_t ram_r8(struct device_s* dev) {
    ram_state_t* ram_dev = (ram_state_t*)dev;
    return ram_dev->data[bus.address];
}

void ram_w8(struct device_s* dev) {
    ram_state_t* ram_dev = (ram_state_t*)dev;
    ram_dev->data[bus.address] = bus.data;
}

// RAM lifecycle methods
static void ram_init(struct device_s* dev) {
    ram_state_t* ram_dev = (ram_state_t*)dev;
    // Initialize RAM state
    memset(ram_dev, 0, sizeof(*ram_dev));
    
    // Set up device callbacks from descriptor pointer
    ram_dev->device = &ram_device_descriptor;
}

static void ram_cycle(struct device_s* dev) {
    // RAM has no cycle logic - just stores data
    (void)dev; // Unused parameter
}

static void ram_cleanup(struct device_s* dev) {
    // RAM has no cleanup needed
    (void)dev; // Unused parameter
}

// Static device descriptor for RAM
static const device_t ram_device_descriptor = {
    .r8 = ram_r8,
    .w8 = ram_w8,
    .init = ram_init,
    .cycle = ram_cycle,
    .cleanup = ram_cleanup
};
