#include "sid.h"
#include <string.h>

// Forward declaration of device descriptor
static const device_t sid_device_descriptor;

// Direct implementation functions for device lifecycle
static void sid_init(struct device_s* dev) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    // Initialize SID state
    memset(sid_dev, 0, sizeof(*sid_dev));
    // Set up device callbacks from descriptor pointer
    sid_dev->device = &sid_device_descriptor;
}

static void sid_cycle(struct device_s* dev) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
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

static void sid_cleanup(struct device_s* dev) { (void)dev; }

// Optimized I/O handlers - called directly via callback table (no chip select checks!)
uint8_t sid_r8(struct device_s* dev, uint16_t address) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    uint8_t reg = address & 0x1F;
    return sid_dev->registers[reg];
}

void sid_w8(struct device_s* dev, uint16_t address, uint8_t data) {
    sid_state_t* sid_dev = (sid_state_t*)dev;
    uint8_t reg = address & 0x1F;
    sid_dev->registers[reg] = data;
}

// Static device descriptor for SID
static const device_t sid_device_descriptor = {
    .r8 = sid_r8,
    .w8 = sid_w8,
    .init = sid_init,
    .cycle = sid_cycle,
    .cleanup = sid_cleanup
};