#include "cia.h"
#include "bus.h"
#include <string.h>

void* cia_system_create(void* bus) {
    cia_t* cia = (cia_t*)calloc(1, sizeof(cia_t));
    if (!cia) return NULL;
    cia->desc = &cia_descriptor;
    cia->bus = (bus_interface_t*)bus;
    return cia;
}

void cia_system_destroy(void* context) {
    free(context);
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
    .read = cia_registers_read,
    .write = cia_registers_write,
    .bank_change = NULL
};

// old

// New optimized I/O handlers - called directly via callback table (no chip select checks!)
uint8_t cia1_r8(struct device_s* dev, uint16_t address) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    uint8_t reg = address & 0x0F;
    switch (reg) {
        case 0x00: return cia_dev->port_a;
        case 0x01: return cia_dev->port_b;
        case 0x04: return cia_dev->timer_a & 0xFF;
        case 0x05: return cia_dev->timer_a >> 8;
        case 0x0D: { uint8_t val = cia_dev->interrupt_status; cia_dev->interrupt_status = 0; return val; }
        default: return 0xFF; // Unmapped registers
    }
}

void cia1_w8(struct device_s* dev, uint16_t address, uint8_t data) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    uint8_t reg = address & 0x0F;
    switch (reg) {
        case 0x00: cia_dev->port_a = data; break;
        case 0x01: cia_dev->port_b = data; break;
        case 0x04: cia_dev->timer_a_latch = (cia_dev->timer_a_latch & 0xFF00) | data; break;
        case 0x05: cia_dev->timer_a_latch = (cia_dev->timer_a_latch & 0x00FF) | (data << 8); break;
        case 0x0E: cia_dev->control_a = data; break;
        case 0x0D: cia_dev->interrupt_control = data; break;
        // Writes to unmapped registers are ignored
    }
}

uint8_t cia2_r8(struct device_s* dev, uint16_t address) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    uint8_t reg = address & 0x0F;
    switch (reg) {
        case 0x00: return cia_dev->port_a;
        case 0x01: return cia_dev->port_b;
        case 0x04: return cia_dev->timer_a & 0xFF;
        case 0x05: return cia_dev->timer_a >> 8;
        case 0x0D: { uint8_t val = cia_dev->interrupt_status; cia_dev->interrupt_status = 0; return val; }
        default: return 0xFF; // Unmapped registers
    }
}

void cia2_w8(struct device_s* dev, uint16_t address, uint8_t data) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    uint8_t reg = address & 0x0F;
    switch (reg) {
        case 0x00: cia_dev->port_a = data; break;
        case 0x01: cia_dev->port_b = data; break;
        case 0x04: cia_dev->timer_a_latch = (cia_dev->timer_a_latch & 0xFF00) | data; break;
        case 0x05: cia_dev->timer_a_latch = (cia_dev->timer_a_latch & 0x00FF) | (data << 8); break;
        case 0x0E: cia_dev->control_a = data; break;
        case 0x0D: cia_dev->interrupt_control = data; break;
        // Writes to unmapped registers are ignored
    }
}

// Forward declarations of device descriptors
static const device_t cia1_device_descriptor;
static const device_t cia2_device_descriptor;

// Direct implementation functions for device lifecycle
static void cia1_init(struct device_s* dev) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    // Initialize CIA1 state
    memset(cia_dev, 0, sizeof(*cia_dev));
    // Set up device callbacks from descriptor pointer
    cia_dev->device = &cia1_device_descriptor;
}

static void cia1_cycle(struct device_s* dev) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    // Timer A always decrements when enabled (hardware accurate)
    if (cia_dev->control_a & 1) {
        if (cia_dev->timer_a == 0) {
            cia_dev->timer_a = cia_dev->timer_a_latch;
            cia_dev->interrupt_status |= 1; // Timer A interrupt
            if (cia_dev->interrupt_control & 1) {
                cia_dev->bus->control_lines |= IRQ_LINE;
            }
        } else {
            cia_dev->timer_a--;
        }
    }
}

static void cia2_init(struct device_s* dev) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    // Initialize CIA2 state
    memset(cia_dev, 0, sizeof(*cia_dev));
    // Set up device callbacks from descriptor pointer
    cia_dev->device = &cia2_device_descriptor;
}

static void cia2_cycle(struct device_s* dev) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    // Timer A always decrements when enabled
    if (cia_dev->control_a & 1) {
        if (cia_dev->timer_a == 0) {
            cia_dev->timer_a = cia_dev->timer_a_latch;
            cia_dev->interrupt_status |= 1;
            if (cia_dev->interrupt_control & 1) {
                cia_dev->bus->control_lines |= NMI_LINE;
            }
        } else {
            cia_dev->timer_a--;
        }
    }
}

// Static device descriptors for CIA1 and CIA2
static const device_t cia1_device_descriptor = {
    .r8 = cia1_r8,
    .w8 = cia1_w8,
    .init = cia1_init,
    .cycle = cia1_cycle,
    .cleanup = NULL
};

static const device_t cia2_device_descriptor = {
    .r8 = cia2_r8,
    .w8 = cia2_w8,
    .init = cia2_init,
    .cycle = cia2_cycle,
    .cleanup = NULL
};

// Attach bus to CIA devices
void cia_attach_bus(cia_state_t* cia_dev, bus_state_t* bus_state) {
    cia_dev->bus = bus_state;
}
