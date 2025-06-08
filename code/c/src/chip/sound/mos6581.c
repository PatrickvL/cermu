#include "mos6581.h" // sid
#include <string.h>
#include <stdlib.h>

void mos6581_system_destroy(void* chip) {
    free(chip);
}

void* mos6581_system_create(chip_descriptor_t* desc) {
    mos6581_t* sid = (mos6581_t*)calloc(1, sizeof(mos6581_t));
    if (!sid) return NULL;
    sid->desc = desc;
    return sid;
}

uint8_t mos6581_registers_read(void* context, uint16_t address) {
    mos6581_t* sid = (mos6581_t*)context;
    // SID has 32 registers that mirror throughout its address space
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

void mos6581_registers_write(void* context, uint16_t address, uint8_t value) {
    mos6581_t* sid = (mos6581_t*)context;
    // SID has 32 registers that mirror throughout its address space
    uint8_t reg = address & 0x1F;
    if (reg <= 0x18) sid->registers[reg] = value;
}

chip_descriptor_t mos6581_descriptor = {
    .create = mos6581_system_create,
    .destroy = mos6581_system_destroy,
    .bus_attach = NULL,
    .read = mos6581_registers_read,
    .write = mos6581_registers_write,
    .bank_change = NULL
};

void mos6581_cycle(mos6581_t* sid) {
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
