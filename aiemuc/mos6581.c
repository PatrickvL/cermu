#include "mos6581.h"
#include "c64.h"
#include <string.h>
#include <stdlib.h>

void mos6581_system_destroy(void* device) {
    free(device);
}

void mos6581_bus_attach(void* device, void* bus) {
    mos6581_t* mos6581 = (mos6581_t*)device;
    c64_bus_t* c64_bus = (c64_bus_t*)bus;
}

void* mos6581_system_create(device_descriptor_t* desc) {
    mos6581_t* mos6581 = (mos6581_t*)calloc(1, sizeof(mos6581_t));
    if (!mos6581) return NULL;
    mos6581->desc = desc;
    return mos6581;
}

uint8_t mos6581_registers_read(void* context, uint16_t address) {
    mos6581_t* mos6581 = (mos6581_t*)context;
    uint8_t reg = address & 0x1F;
    if (reg >= 0x19 && reg <= 0x1C) {
        switch (reg) {
            case 0x19: return mos6581->pot_x;
            case 0x1A: return mos6581->pot_y;
            case 0x1B: return mos6581->osc3;
            case 0x1C: return mos6581->env3;
        }
    }
    return 0;
}

void mos6581_registers_write(void* context, uint16_t address, uint8_t value) {
    mos6581_t* mos6581 = (mos6581_t*)context;
    uint8_t reg = address & 0x1F;
    if (reg <= 0x18) mos6581->registers[reg] = value;
}

device_descriptor_t mos6581_descriptor = {
    .create = mos6581_system_create,
    .destroy = mos6581_system_destroy,
    .bus_attach = mos6581_bus_attach,
    .read = mos6581_registers_read,
    .write = mos6581_registers_write,
    .bank_change = NULL
};

static void mos6581_cycle(mos6581_t* mos6581) {
    // Envelope generators always run (hardware accurate)
    for (int voice = 0; voice < 3; voice++) {
        mos6581->envelope_counter[voice]++;
        if (mos6581->envelope_counter[voice] >= 0x8000) {
            mos6581->envelope_counter[voice] = 0;
            // Simplified envelope state machine
            mos6581->envelope_state[voice] = (mos6581->envelope_state[voice] + 1) & 0xFF;
        }
    }
}
