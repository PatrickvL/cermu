#include "sid.h"
#include "bus.h"

sid_state_t sid;

// Optimized SID cycle function - no I/O handling, just envelope generators
void sid_cycle(void) {
    // Envelope generators always run (hardware accurate)
    for (int voice = 0; voice < 3; voice++) {
        sid.envelope_counter[voice]++;
        if (sid.envelope_counter[voice] >= 0x8000) {
            sid.envelope_counter[voice] = 0;
            // Simplified envelope state machine
            sid.envelope_state[voice] = (sid.envelope_state[voice] + 1) & 0xFF;
        }
    }
}

// New optimized I/O handlers - called directly via callback table (no chip select checks!)
void sid_handle_read(void) {
    uint8_t reg = bus_state.address & 0x1F;
    bus_state.data = sid.registers[reg];
}

void sid_handle_write(void) {
    uint8_t reg = bus_state.address & 0x1F;
    sid.registers[reg] = bus_state.data;
}