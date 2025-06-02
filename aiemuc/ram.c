#include "ram.h"
#include <string.h>

void* ram_system_create(void* bus) {
    ram_t* ram = (ram_t*)calloc(1, sizeof(ram_t));
    if (!ram) return NULL;
    ram->desc = &ram_descriptor;
    ram->bus = (bus_interface_t*)bus;
    return ram;
}

void ram_system_destroy(void* context) {
    free(context);
}

uint8_t ram_memory_read(void* context, uint16_t address) {
    uint8_t* memory = (uint8_t*)context;
    return memory[address];
}

void ram_memory_write(void* context, uint16_t address, uint8_t value) {
    uint8_t* memory = (uint8_t*)context;
    memory[address] = value;
}

static device_descriptor_t ram_descriptor = {
    .create = ram_system_create,
    .destroy = ram_system_destroy,
    .read = ram_memory_read,
    .write = ram_memory_write,
    .bank_change = NULL
};

// old

// Forward declaration of descriptor
static const device_t ram_device_descriptor;

// RAM I/O handlers - called directly via callback table (no chip select checks!)
uint8_t ram_r8(struct device_s* dev, uint16_t address) {
    ram_state_t* ram_dev = (ram_state_t*)dev;
    return ram_dev->data[address];
}

void ram_w8(struct device_s* dev, uint16_t address, uint8_t data) {
    ram_state_t* ram_dev = (ram_state_t*)dev;
    ram_dev->data[address] = data;
}

// RAM lifecycle methods
static void ram_init(struct device_s* dev) {
    ram_state_t* ram_dev = (ram_state_t*)dev;
    // Initialize RAM state
    memset(ram_dev, 0, sizeof(*ram_dev));
    
    // Set up device callbacks from descriptor pointer
    ram_dev->device = &ram_device_descriptor;
}

// Static device descriptor for RAM
static const device_t ram_device_descriptor = {
    .r8 = ram_r8,
    .w8 = ram_w8,
    .init = ram_init,
    .cycle = NULL, // RAM has no cycle logic - just stores data
    .cleanup = NULL // RAM has no cleanup needed
};
