#include "cia.h"
#include "bus.h"
#include <string.h>

cia_state_t cia1, cia2;

// CIA1 initialization
void cia1_init(void) {
    // Initialize CIA1 state
    memset(&cia1, 0, sizeof(cia1));
    
    // Set up device callbacks
    cia1.device.r8 = cia1_r8;
    cia1.device.w8 = cia1_w8;
}

// CIA2 initialization
void cia2_init(void) {
    // Initialize CIA2 state
    memset(&cia2, 0, sizeof(cia2));
    
    // Set up device callbacks
    cia2.device.r8 = cia2_r8;
    cia2.device.w8 = cia2_w8;
}

// Optimized CIA cycle functions - no I/O handling, just timers and logic
void cia1_cycle(void) {
    // Timer A always decrements when enabled (hardware accurate)
    if (cia1.control_a & 1) {
        if (cia1.timer_a == 0) {
            cia1.timer_a = cia1.timer_a_latch;
            cia1.interrupt_status |= 1; // Timer A interrupt
            if (cia1.interrupt_control & 1) {
                bus.control_lines |= IRQ_LINE;
            }
        } else {
            cia1.timer_a--;
        }
    }
}

void cia2_cycle(void) {
    // Timer A always decrements when enabled
    if (cia2.control_a & 1) {
        if (cia2.timer_a == 0) {
            cia2.timer_a = cia2.timer_a_latch;
            cia2.interrupt_status |= 1;
            if (cia2.interrupt_control & 1) {
                bus.control_lines |= NMI_LINE;
            }
        } else {
            cia2.timer_a--;
        }
    }
}

// New optimized I/O handlers - called directly via callback table (no chip select checks!)
uint8_t cia1_r8(void) {
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: return cia1.port_a;
        case 0x01: return cia1.port_b;
        case 0x04: return cia1.timer_a & 0xFF;
        case 0x05: return cia1.timer_a >> 8;
        case 0x0D: { uint8_t val = cia1.interrupt_status; cia1.interrupt_status = 0; return val; }
        default: return 0xFF; // Unmapped registers
    }
}

void cia1_w8(void) {
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: cia1.port_a = bus.data; break;
        case 0x01: cia1.port_b = bus.data; break;
        case 0x04: cia1.timer_a_latch = (cia1.timer_a_latch & 0xFF00) | bus.data; break;
        case 0x05: cia1.timer_a_latch = (cia1.timer_a_latch & 0x00FF) | (bus.data << 8); break;
        case 0x0E: cia1.control_a = bus.data; break;
        case 0x0D: cia1.interrupt_control = bus.data; break;
        // Writes to unmapped registers are ignored
    }
}

uint8_t cia2_r8(void) {
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: return cia2.port_a;
        case 0x01: return cia2.port_b;
        case 0x04: return cia2.timer_a & 0xFF;
        case 0x05: return cia2.timer_a >> 8;
        case 0x0D: { uint8_t val = cia2.interrupt_status; cia2.interrupt_status = 0; return val; }
        default: return 0xFF; // Unmapped registers
    }
}

void cia2_w8(void) {
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: cia2.port_a = bus.data; break;
        case 0x01: cia2.port_b = bus.data; break;
        case 0x04: cia2.timer_a_latch = (cia2.timer_a_latch & 0xFF00) | bus.data; break;
        case 0x05: cia2.timer_a_latch = (cia2.timer_a_latch & 0x00FF) | (bus.data << 8); break;
        case 0x0E: cia2.control_a = bus.data; break;
        case 0x0D: cia2.interrupt_control = bus.data; break;
        // Writes to unmapped registers are ignored
    }
}