#include "chip.h"

// ============================================================================
// GENERIC STUB FUNCTIONS
// ============================================================================

/**
 * Generic stub read function for unattached callbacks.
 * Returns 0xFF for any read operation to provide consistent behavior.
 * Use this to eliminate null checks in high-frequency code paths.
 * 
 * @param context Unused context pointer (can be NULL)
 * @param address Unused address parameter
 * @return Always returns 0xF
 */
uint8_t generic_stub_read(void* context, uint16_t address) {
    (void)context;
    (void)address;
    return 0xFF; // Return consistent value for unattached reads
}

/**
 * Generic stub write function for unattached callbacks.
 * Does nothing for any write operation.
 * Use this to eliminate null checks in high-frequency code paths.
 * 
 * @param context Unused context pointer (can be NULL)
 * @param address Unused address parameter
 * @param value Unused value parameter
 */
void generic_stub_write(void* context, uint16_t address, uint8_t value) {
    (void)context;
    (void)address;
    (void)value;
    // Do nothing - stub for unattached writes
}
