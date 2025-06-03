#include "mos6526.h"
#include "c64_bus.h"
#include <string.h>
#include <stdlib.h>

void* mos6526_system_create(device_descriptor_t* desc) {
    mos6526_t* mos6526 = (mos6526_t*)calloc(1, sizeof(mos6526_t));
    if (!mos6526) return NULL;
    mos6526->desc = desc;
    return mos6526;
}

void mos6526_system_destroy(void* device) {
    free(device);
}

void mos6526_bus_attach(void* device, c64_bus_t* bus) {
    mos6526_t* mos6526 = (mos6526_t*)device;
    mos6526->bus = bus;
}

uint8_t mos6526_registers_read(void* context, uint16_t address) {
    mos6526_t* mos6526 = (mos6526_t*)context;
    uint8_t reg = address & 0xF;
    switch (reg) {
        case 0x0: return mos6526->pra & mos6526->ddra;
        case 0x1: return mos6526->prb & mos6526->ddrb;
        case 0x2: return mos6526->ddra;
        case 0x3: return mos6526->ddrb;
        case 0x4: return mos6526->timer_a & 0xFF;
        case 0x5: return mos6526->timer_a >> 8;
        case 0x6: return mos6526->timer_b & 0xFF;
        case 0x7: return mos6526->timer_b >> 8;
        case 0x8:
            if (!mos6526->tod_latched) {
                mos6526->tod_latch[0] = mos6526->tod_10ths;
                mos6526->tod_latch[1] = mos6526->tod_sec;
                mos6526->tod_latch[2] = mos6526->tod_min;
                mos6526->tod_latch[3] = mos6526->tod_hr;
                mos6526->tod_latched = true;
            }
            return mos6526->tod_latch[0];
        case 0x9: return mos6526->tod_latched ? mos6526->tod_latch[1] : mos6526->tod_sec;
        case 0xA: return mos6526->tod_latched ? mos6526->tod_latch[2] : mos6526->tod_min;
        case 0xB: mos6526->tod_latched = false; return mos6526->tod_latched ? mos6526->tod_latch[3] : mos6526->tod_hr;
        case 0xC: return 0;
        case 0xD: {
            uint8_t status = mos6526->icr;
            mos6526->icr = 0;
            return status;
        }
        case 0xE: return mos6526->cra;
        case 0xF: return mos6526->crb;
    }
    return 0;
}

void mos6526_registers_write(void* context, uint16_t address, uint8_t value) {
    mos6526_t* mos6526 = (mos6526_t*)context;
    uint8_t reg = address & 0xF;
    switch (reg) {
        case 0x0:
            mos6526->pra = value;
            if (address >= 0xDD00) {
                c64_bus_memory_write(mos6526->bus, address, value);
            }
            break;
        case 0x1: mos6526->prb = value; break;
        case 0x2: mos6526->ddra = value; break;
        case 0x3: mos6526->ddrb = value; break;
        case 0x4: mos6526->timer_a = (mos6526->timer_a & 0xFF00) | value; break;
        case 0x5: mos6526->timer_a = (mos6526->timer_a & 0xFF) | (value << 8); break;
        case 0x6: mos6526->timer_b = (mos6526->timer_b & 0xFF00) | value; break;
        case 0x7: mos6526->timer_b = (mos6526->timer_b & 0xFF) | (value << 8); break;
        case 0x8: mos6526->tod_10ths = value & 0xF; mos6526->tod_latched = false; break;
        case 0x9: mos6526->tod_sec = value & 0x7F; break;
        case 0xA: mos6526->tod_min = value & 0x7F; break;
        case 0xB: mos6526->tod_hr = value & 0x1F; break;
        case 0xC: mos6526->sdr = value; break;
        case 0xD: mos6526->icr = value; break;
        case 0xE: mos6526->cra = value; break;
        case 0xF: mos6526->crb = value; break;
    }
}

device_descriptor_t mos6526_descriptor = {
    .create = mos6526_system_create,
    .destroy = mos6526_system_destroy,
    .bus_attach = mos6526_bus_attach,
    .read = mos6526_registers_read,
    .write = mos6526_registers_write,
    .bank_change = NULL
};

static void mos6526_cycle(mos6526_t* mos6526) {
    // Timer A always decrements when enabled (hardware accurate)
    if (mos6526->cra & 1) {
        if (mos6526->timer_a == 0) {
/*
            mos6526->timer_a = mos6526->timer_a_latch;
*/            
            mos6526->icr |= 1; // Timer A interrupt
            if (mos6526->sdr & 1) {
                mos6526->bus->control_lines |= IRQ_LINE;
            }
        } else {
            mos6526->timer_a--;
        }
    }
}