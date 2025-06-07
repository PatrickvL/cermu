#include "c64_bus.h"
#include "c64.h"
#include <stdlib.h>

uint8_t c64_bus_memory_read(void* device, uint16_t address) {
    c64_bus_t* c64_bus = (c64_bus_t*)device;
    int index = c64_bus_get_device_index(address);
    uint8_t id = DEVID_READ_DECODE(c64_bus->device_id_per_page[index]);
    device_access_callback_t* cb = &c64_bus->device_access_callbacks[id];
    return cb->read_func(cb->context, address);
}

void c64_bus_memory_write(void* context, uint16_t address, uint8_t value) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    int index = c64_bus_get_device_index(address);
    uint8_t id = DEVID_WRITE_DECODE(c64_bus->device_id_per_page[index]);
    device_access_callback_t* cb = &c64_bus->device_access_callbacks[id];
    cb->write_func(cb->context, address, value);
    // TODO : Move below signalling of VIC-II bank change to somewhere else with less impact on performance
    if (address == 0xDD00) {
        c64_t* c64 = c64_bus->c64;
        uint8_t bank = 3 - (value & 0x3);
        if (c64->sid->desc->bank_change) {
            c64->sid->desc->bank_change(c64->sid, bank);
        }
    }
}

void c64_bus_system_destroy(void* device) {
    free(device);
}

void* c64_bus_system_create(device_descriptor_t* desc) {
    c64_bus_t* c64_bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!c64_bus) return NULL;
    c64_bus->desc = desc;
    // Initialize bus state
    c64_bus->address = 0;
    c64_bus->data = 0;
    c64_bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
    return c64_bus;
}

void c64_bus_system_attach(c64_bus_t* c64_bus, c64_t* c64) {
    c64_bus->c64 = c64;
}

device_descriptor_t c64_bus_descriptor = {
    .create = c64_bus_system_create,
    .destroy = c64_bus_system_destroy,
    .bus_attach = NULL,
    .read = c64_bus_memory_read,
    .write = c64_bus_memory_write,
    .bank_change = NULL
};

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode) {
    c64_bus->device_id_per_page = c64_bus->device_id_per_index_per_mode[mode];
}

uint8_t c64_bus_read_cycle(c64_bus_t *c64_bus, uint16_t addr) {
    c64_bus->address = addr; // Perhaps this is no longer needed
    uint8_t data = c64_bus_memory_read(c64_bus, addr);
    c64_bus->data = data; // Perhaps this is no longer needed
    c64_non_cpu_cycle(c64_bus->c64);
    return data;
}    

void c64_bus_write_cycle(c64_bus_t* c64_bus, uint16_t addr, uint8_t value) {
    c64_bus->address = addr; // Perhaps this is no longer needed
    c64_bus->data = value; // Perhaps this is no longer needed
    c64_bus_memory_write(c64_bus, addr, value);
    c64_non_cpu_cycle(c64_bus->c64);
}        
