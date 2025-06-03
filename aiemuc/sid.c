#include "sid.h"
#include "c64.h"
#include <string.h>
#include <stdlib.h>

void sid_system_destroy(void* device) {
    free(device);
}

void sid_bus_attach(void* device, void* bus) {
    sid_t* sid = (sid_t*)device;
    c64_bus_t* c64_bus = (c64_bus_t*)bus;
}

void* sid_system_create(device_descriptor_t* desc) {
    sid_t* sid = (sid_t*)calloc(1, sizeof(sid_t));
    if (!sid) return NULL;
    sid->desc = desc;
    return sid;
}

uint8_t sid_registers_read(void* context, uint16_t address) {
    sid_t* sid = (sid_t*)context;
    uint8_t reg = address & 0x1F;
    if (reg >= 0x19 && reg <= 0x1C) {
        switch (reg) {
            case 0x19: return sid->pot_x;
            case 0x1A: return sid->pot_y;
            case 0x1B: return sid->osc3;
            case 0x1C: return sid->env3;
        }
    }
    return 0;
}

void sid_registers_write(void* context, uint16_t address, uint8_t value) {
    sid_t* sid = (sid_t*)context;
    uint8_t reg = address & 0x1F;
    if (reg <= 0x18) sid->registers[reg] = value;
}

device_descriptor_t sid_descriptor = {
    .create = sid_system_create,
    .destroy = sid_system_destroy,
    .bus_attach = sid_bus_attach,
    .read = sid_registers_read,
    .write = sid_registers_write,
    .bank_change = NULL
};

static void sid_cycle(sid_t* sid) {
    // Envelope generators always run (hardware accurate)
    for (int voice = 0; voice < 3; voice++) {
        sid->envelope_counter[voice]++;
        if (sid->envelope_counter[voice] >= 0x8000) {
            sid->envelope_counter[voice] = 0;
            // Simplified envelope state machine
            sid->envelope_state[voice] = (sid->envelope_state[voice] + 1) & 0xFF;
        }
    }
}
