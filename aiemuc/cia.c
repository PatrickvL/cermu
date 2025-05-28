#include "cia.h"
#include "bus.h"
#include <string.h>

// Optimized CIA cycle functions - no I/O handling, just timers and logic
void cia1_cycle(cia_state_t* cia_dev) {
    // Timer A always decrements when enabled (hardware accurate)
    if (cia_dev->control_a & 1) {
        if (cia_dev->timer_a == 0) {
            cia_dev->timer_a = cia_dev->timer_a_latch;
            cia_dev->interrupt_status |= 1; // Timer A interrupt
            if (cia_dev->interrupt_control & 1) {
                bus.control_lines |= IRQ_LINE;
            }
        } else {
            cia_dev->timer_a--;
        }
    }
}

void cia2_cycle(cia_state_t* cia_dev) {
    // Timer A always decrements when enabled
    if (cia_dev->control_a & 1) {
        if (cia_dev->timer_a == 0) {
            cia_dev->timer_a = cia_dev->timer_a_latch;
            cia_dev->interrupt_status |= 1;
            if (cia_dev->interrupt_control & 1) {
                bus.control_lines |= NMI_LINE;
            }
        } else {
            cia_dev->timer_a--;
        }
    }
}

// New optimized I/O handlers - called directly via callback table (no chip select checks!)
uint8_t cia1_r8(struct device_s* dev) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: return cia_dev->port_a;
        case 0x01: return cia_dev->port_b;
        case 0x04: return cia_dev->timer_a & 0xFF;
        case 0x05: return cia_dev->timer_a >> 8;
        case 0x0D: { uint8_t val = cia_dev->interrupt_status; cia_dev->interrupt_status = 0; return val; }
        default: return 0xFF; // Unmapped registers
    }
}

void cia1_w8(struct device_s* dev) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: cia_dev->port_a = bus.data; break;
        case 0x01: cia_dev->port_b = bus.data; break;
        case 0x04: cia_dev->timer_a_latch = (cia_dev->timer_a_latch & 0xFF00) | bus.data; break;
        case 0x05: cia_dev->timer_a_latch = (cia_dev->timer_a_latch & 0x00FF) | (bus.data << 8); break;
        case 0x0E: cia_dev->control_a = bus.data; break;
        case 0x0D: cia_dev->interrupt_control = bus.data; break;
        // Writes to unmapped registers are ignored
    }
}

uint8_t cia2_r8(struct device_s* dev) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: return cia_dev->port_a;
        case 0x01: return cia_dev->port_b;
        case 0x04: return cia_dev->timer_a & 0xFF;
        case 0x05: return cia_dev->timer_a >> 8;
        case 0x0D: { uint8_t val = cia_dev->interrupt_status; cia_dev->interrupt_status = 0; return val; }
        default: return 0xFF; // Unmapped registers
    }
}

void cia2_w8(struct device_s* dev) {
    cia_state_t* cia_dev = (cia_state_t*)dev;
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: cia_dev->port_a = bus.data; break;
        case 0x01: cia_dev->port_b = bus.data; break;
        case 0x04: cia_dev->timer_a_latch = (cia_dev->timer_a_latch & 0xFF00) | bus.data; break;
        case 0x05: cia_dev->timer_a_latch = (cia_dev->timer_a_latch & 0x00FF) | (bus.data << 8); break;
        case 0x0E: cia_dev->control_a = bus.data; break;
        case 0x0D: cia_dev->interrupt_control = bus.data; break;
        // Writes to unmapped registers are ignored
    }
}

// CIA1 initialization
void cia1_init(cia_state_t* cia_dev) {
    // Initialize CIA1 state
    memset(cia_dev, 0, sizeof(*cia_dev));
    
    // Set up device callbacks
    cia_dev->device.r8 = cia1_r8;
    cia_dev->device.w8 = cia1_w8;
}

// CIA2 initialization
void cia2_init(cia_state_t* cia_dev) {
    // Initialize CIA2 state
    memset(cia_dev, 0, sizeof(*cia_dev));
    
    // Set up device callbacks
    cia_dev->device.r8 = cia2_r8;
    cia_dev->device.w8 = cia2_w8;
}
