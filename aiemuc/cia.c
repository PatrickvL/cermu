#include "cia.h"
#include "bus.h"

cia_state_t cia1, cia2;

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
void cia1_handle_read(void) {
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: bus.data = cia1.port_a; break;
        case 0x01: bus.data = cia1.port_b; break;
        case 0x04: bus.data = cia1.timer_a & 0xFF; break;
        case 0x05: bus.data = cia1.timer_a >> 8; break;
        case 0x0D: bus.data = cia1.interrupt_status; cia1.interrupt_status = 0; break;
        default: bus.data = 0xFF; break; // Unmapped registers
    }
}

void cia1_handle_write(void) {
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

void cia2_handle_read(void) {
    uint8_t reg = bus.address & 0x0F;
    switch (reg) {
        case 0x00: bus.data = cia2.port_a; break;
        case 0x01: bus.data = cia2.port_b; break;
        case 0x04: bus.data = cia2.timer_a & 0xFF; break;
        case 0x05: bus.data = cia2.timer_a >> 8; break;
        case 0x0D: bus.data = cia2.interrupt_status; cia2.interrupt_status = 0; break;
        default: bus.data = 0xFF; break; // Unmapped registers
    }
}

void cia2_handle_write(void) {
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