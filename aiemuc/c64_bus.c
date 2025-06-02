#include "c64_bus.h"

// Bus functions
void* c64_bus_system_create(void* bus) {
    c64_bus_t* c64_bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!c64_bus) return NULL;
    c64_bus->desc = &c64_bus_descriptor;
    c64_bus->interface.read = c64_bus_memory_read;
    c64_bus->interface.write = c64_bus_memory_write;
    c64_bus->c64 = (c64_state_t*)bus;
    return c64_bus;
}

void c64_bus_system_destroy(void* context) {
    free(context);
}

uint8_t c64_bus_memory_read(void* context, uint16_t address) {
    c64_bus_t* bus = (c64_bus_t*)context;
    return c64_memory_read(bus->c64, address);
}

void c64_bus_memory_write(void* context, uint16_t address, uint8_t value) {
    c64_bus_t* bus = (c64_bus_t*)context;
    c64_state_t* c64 = bus->c64;
    if (address == 0xDD00) {
        uint8_t bank = 3 - (value & 0x3);
        if (c64->vic_ii->desc->bank_change) {
            c64->vic_ii->desc->bank_change(c64->vic_ii, bank);
        }
    }
    c64_memory_write(c64, address, value);
}

static device_descriptor_t c64_bus_descriptor = {
    .create = c64_bus_system_create,
    .destroy = c64_bus_system_destroy,
    .read = c64_bus_memory_read,
    .write = c64_bus_memory_write,
    .bank_change = NULL
};
