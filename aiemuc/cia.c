#include "cia.h"
#include "c64_bus.h"
#include <string.h>

void* cia_system_create(device_descriptor_t* desc) {
    cia_t* cia = (cia_t*)calloc(1, sizeof(cia_t));
    if (!cia) return NULL;
    cia->desc = desc;
    return cia;
}

void cia_system_destroy(void* device) {
    free(device);
}

void cia_bus_attach(void* device, c64_bus_t* bus) {
    cia_t* cia = (cia_t*)device;
    cia->bus = bus;
}

uint8_t cia_registers_read(void* context, uint16_t address) {
    cia_t* cia = (cia_t*)context;
    uint8_t reg = address & 0xF;
    switch (reg) {
        case 0x0: return cia->pra & cia->ddra;
        case 0x1: return cia->prb & cia->ddrb;
        case 0x2: return cia->ddra;
        case 0x3: return cia->ddrb;
        case 0x4: return cia->timer_a & 0xFF;
        case 0x5: return cia->timer_a >> 8;
        case 0x6: return cia->timer_b & 0xFF;
        case 0x7: return cia->timer_b >> 8;
        case 0x8:
            if (!cia->tod_latched) {
                cia->tod_latch[0] = cia->tod_10ths;
                cia->tod_latch[1] = cia->tod_sec;
                cia->tod_latch[2] = cia->tod_min;
                cia->tod_latch[3] = cia->tod_hr;
                cia->tod_latched = true;
            }
            return cia->tod_latch[0];
        case 0x9: return cia->tod_latched ? cia->tod_latch[1] : cia->tod_sec;
        case 0xA: return cia->tod_latched ? cia->tod_latch[2] : cia->tod_min;
        case 0xB: cia->tod_latched = false; return cia->tod_latched ? cia->tod_latch[3] : cia->tod_hr;
        case 0xC: return 0;
        case 0xD: {
            uint8_t status = cia->icr;
            cia->icr = 0;
            return status;
        }
        case 0xE: return cia->cra;
        case 0xF: return cia->crb;
    }
    return 0;
}

void cia_registers_write(void* context, uint16_t address, uint8_t value) {
    cia_t* cia = (cia_t*)context;
    uint8_t reg = address & 0xF;
    switch (reg) {
        case 0x0:
            cia->pra = value;
            if (address >= 0xDD00 && cia->bus->write) {
                cia->bus->write(cia->bus, address, value);
            }
            break;
        case 0x1: cia->prb = value; break;
        case 0x2: cia->ddra = value; break;
        case 0x3: cia->ddrb = value; break;
        case 0x4: cia->timer_a = (cia->timer_a & 0xFF00) | value; break;
        case 0x5: cia->timer_a = (cia->timer_a & 0xFF) | (value << 8); break;
        case 0x6: cia->timer_b = (cia->timer_b & 0xFF00) | value; break;
        case 0x7: cia->timer_b = (cia->timer_b & 0xFF) | (value << 8); break;
        case 0x8: cia->tod_10ths = value & 0xF; cia->tod_latched = false; break;
        case 0x9: cia->tod_sec = value & 0x7F; break;
        case 0xA: cia->tod_min = value & 0x7F; break;
        case 0xB: cia->tod_hr = value & 0x1F; break;
        case 0xC: cia->sdr = value; break;
        case 0xD: cia->icr = value; break;
        case 0xE: cia->cra = value; break;
        case 0xF: cia->crb = value; break;
    }
}

static device_descriptor_t cia_descriptor = {
    .create = cia_system_create,
    .destroy = cia_system_destroy,
    .bus_attach = cia_bus_attach,
    .read = cia_registers_read,
    .write = cia_registers_write,
    .bank_change = NULL
};

static void cia_cycle(cia_t* cia) {
    // Timer A always decrements when enabled (hardware accurate)
    if (cia->cra & 1) {
        if (cia->timer_a == 0) {
/*
            cia->timer_a = cia->timer_a_latch;
*/            
            cia->icr |= 1; // Timer A interrupt
            if (cia->sdr & 1) {
                cia->c64_bus->control_lines |= IRQ_LINE;
            }
        } else {
            cia->timer_a--;
        }
    }
}