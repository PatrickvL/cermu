#include "sid.h"
#include "bus.h"

sid_state_t sid;

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
    
    // Handle CPU register access when chip selected
    if (unlikely(bus_state.chip_selects & SID_CS)) {
        uint8_t reg = bus_state.address & 0x1F;
        if (bus_state.control_lines & WRITE_CYCLE) {
            sid.registers[reg] = bus_state.data;
        } else {
            bus_state.data = sid.registers[reg];
        }
    }
}