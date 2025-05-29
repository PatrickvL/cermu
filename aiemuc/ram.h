#ifndef RAM_H
#define RAM_H

#include "bus.h"

// ============================================================================
// RAM EMULATION - 64K system RAM with CPU port handling
// ============================================================================

typedef struct {
    const device_t* device;  // Pointer to device descriptor (must be first)
    uint8_t data[65536];  // 64K RAM
} ram_state_t;

// Forward declaration
struct device_s;

// Note: Use device_init(), device_cycle(), device_cleanup() instead of wrapper functions

#endif // RAM_H