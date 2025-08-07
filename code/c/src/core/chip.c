#include "chip.h"

/**
 * Generic stub callback function for unattached chip callbacks.
 * Returns bus_state with data=0x00 for read operations, passes through unchanged for writes.
 * Use this to eliminate null checks in high-frequency code paths.
 */
bus_state_t generic_stub_callback(void* context, bus_state_t bus_state) {
    (void)context; // Unused parameter
    // For read operations (indicated by R/W line being high), return 0x00
    // For write operations, just pass through unchanged
    if (bus_state.lines & BUS_MASK_RW) {
        bus_state.data = 0x00; // Read operation - return 0x00
    }
    // Write operations pass through unchanged
    return bus_state;
}

// Legacy stub functions for backward compatibility (deprecated)
uint8_t generic_stub_read(void* context, uint16_t address) {
    (void)context; // Unused parameter
    (void)address; // Unused parameter
    return 0x00;
}

void generic_stub_write(void* context, uint16_t address, uint8_t value) {
    (void)context; // Unused parameter
    (void)address; // Unused parameter
    (void)value;   // Unused parameter
    // Do nothing for write operations
}
