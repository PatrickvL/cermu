#include "cia.h"
#include "bus.h"
#include <string.h>

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
