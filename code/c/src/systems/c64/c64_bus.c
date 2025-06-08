#include "c64_bus.h"
#include "c64.h"
#include <stdlib.h>
#include <string.h>

// Inline function to calculate condensed bank index from address.
// Bank number is derived from the upper 4 bits of the address, whereby the lower 4 
// IO range (bank 13), returns 16 + the page number (from the 2nd address nybble).
static inline int c64_bus_address_to_bankidx(uint16_t address) {
    int page = (address >> 8) & 0x0F;
    int bank = address >> 12; // bank 0 to 15 (13 is unused)
    int bank13_delta = 3 + page; // 3 to 18, when added to 13 gives 16 to 31
    int is_bank13_mask = - (int)(bank == 13); // 0x00000000 or 0xFFFFFFFF
    return bank + (is_bank13_mask & bank13_delta); // 0 to 31: 0 to 15 bank numbers (13 unused), 16 and up for bank 13 pages
}

uint8_t c64_bus_memory_read(c64_bus_t* c64_bus, uint16_t address) {
    int bankidx = c64_bus_address_to_bankidx(address);
    uint8_t devid = DEVID_READ_DECODE(c64_bus->devid_per_bankidx[bankidx]);
    access_callback_t* cb = &c64_bus->access_callback_per_devid[devid];
    return cb->read_func(cb->context, address);
}

void c64_bus_memory_write(c64_bus_t* c64_bus, uint16_t address, uint8_t value) {
    int bankidx = c64_bus_address_to_bankidx(address);
    uint8_t devid = DEVID_WRITE_DECODE(c64_bus->devid_per_bankidx[bankidx]);
    access_callback_t* cb = &c64_bus->access_callback_per_devid[devid];
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

void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64) {
    c64_bus->c64 = c64;  // Store as opaque pointer
}

device_descriptor_t c64_bus_descriptor = {
    .create = c64_bus_system_create,
    .destroy = c64_bus_system_destroy,
    .bus_attach = NULL,
    .read = NULL,
    .write = NULL,
    .bank_change = NULL
};

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode) {
    // Update the device ID mapping for the current mode
    memcpy(c64_bus->devid_per_bankidx, c64_bus->devid_per_bankidx_per_mode[mode], sizeof(c64_bus->devid_per_bankidx));
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
