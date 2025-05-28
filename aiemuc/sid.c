#include "sid.h"
#include "bus.h"
#include <string.h>

sid_state_t sid;

// SID initialization
void sid_init(sid_state_t* sid_dev) {
    // Initialize SID state
    memset(sid_dev, 0, sizeof(*sid_dev));
    
    // Set up device callbacks
    sid_dev->device.r8 = sid_r8;
    sid_dev->device.w8 = sid_w8;
}

// Optimized SID cycle function - no I/O handling, just envelope generators
void sid_cycle(sid_state_t* sid_dev) {
    // Envelope generators always run (hardware accurate)
    for (int voice = 0; voice < 3; voice++) {
        sid_dev->envelope_counter[voice]++;
        if (sid_dev->envelope_counter[voice] >= 0x8000) {
            sid_dev->envelope_counter[voice] = 0;
            // Simplified envelope state machine
            sid_dev->envelope_state[voice] = (sid_dev->envelope_state[voice] + 1) & 0xFF;
        }
    }
}

// New optimized I/O handlers - called directly via callback table (no chip select checks!)
uint8_t sid_r8(struct device_s* dev) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    uint8_t reg = bus.address & 0x1F;
    return sid_dev->registers[reg];
}

void sid_w8(struct device_s* dev) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    uint8_t reg = bus.address & 0x1F;
    sid_dev->registers[reg] = bus.data;
}