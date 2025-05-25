#include "cia.h"
#include "bus.h"

cia_state_t cia1, cia2;

void cia1_cycle(void) {
    // Timer A always decrements when enabled (hardware accurate)
    if (cia1.control_a & 1) {
        if (cia1.timer_a == 0) {
            cia1.timer_a = cia1.timer_a_latch;
            cia1.interrupt_status |= 1; // Timer A interrupt
            if (cia1.interrupt_control & 1) {
                bus_state.control_lines |= IRQ_LINE;
            }
        } else {
            cia1.timer_a--;
        }
    }
    
    // Handle CPU register access when chip selected
    if (unlikely(bus_state.chip_selects & CIA1_CS)) {
        uint8_t reg = bus_state.address & 0x0F;
        if (bus_state.control_lines & WRITE_CYCLE) {
            switch (reg) {
                case 0x00: cia1.port_a = bus_state.data; break;
                case 0x01: cia1.port_b = bus_state.data; break;  
                case 0x04: cia1.timer_a_latch = (cia1.timer_a_latch & 0xFF00) | bus_state.data; break;
                case 0x05: cia1.timer_a_latch = (cia1.timer_a_latch & 0x00FF) | (bus_state.data << 8); break;
                case 0x0E: cia1.control_a = bus_state.data; break;
                case 0x0D: cia1.interrupt_control = bus_state.data; break;
            }
        } else {
            switch (reg) {
                case 0x00: bus_state.data = cia1.port_a; break;
                case 0x01: bus_state.data = cia1.port_b; break;
                case 0x04: bus_state.data = cia1.timer_a & 0xFF; break;
                case 0x05: bus_state.data = cia1.timer_a >> 8; break;
                case 0x0D: bus_state.data = cia1.interrupt_status; cia1.interrupt_status = 0; break;
            }
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
                bus_state.control_lines |= NMI_LINE;
            }
        } else {
            cia2.timer_a--;
        }
    }
    
    // Handle register access (similar to CIA1, abbreviated for space)
    if (unlikely(bus_state.chip_selects & CIA2_CS)) {
        // Register access logic similar to CIA1...
    }
}