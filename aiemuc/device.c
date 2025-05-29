#include "device.h"

// Default no-op handlers for unmapped areas
uint8_t nop_r8(struct device_s* dev) {
    (void)dev; // Unused parameter
    return 0xFF; // Default read value for unmapped areas
}

void nop_w8(struct device_s* dev) {
    (void)dev; // Unused parameter
    // Writes to unmapped areas are ignored
}

// Helper function to extract read callback from pointer-based device
uint8_t (*get_device_read_callback(struct device_s* dev))(struct device_s*) {
    if (!dev) return nop_r8;
    
    // Since our devices now use pointer-based descriptors, we need to cast appropriately
    // All our device structures have 'const device_t* device' as the first field
    const device_t* device_desc = *(const device_t**)dev;
    return device_desc ? device_desc->r8 : nop_r8;
}

// Helper function to extract write callback from pointer-based device
void (*get_device_write_callback(struct device_s* dev))(struct device_s*) {
    if (!dev) return nop_w8;
    
    // Since our devices now use pointer-based descriptors, we need to cast appropriately
    // All our device structures have 'const device_t* device' as the first field
    const device_t* device_desc = *(const device_t**)dev;
    return device_desc ? device_desc->w8 : nop_w8;
}