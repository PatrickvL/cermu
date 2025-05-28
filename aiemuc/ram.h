#ifndef RAM_H
#define RAM_H

#include "bus.h"

// ============================================================================
// RAM EMULATION - 64K system RAM with CPU port handling
// ============================================================================

typedef struct {
    device_t device;  // Generic device interface (must be first)
    uint8_t data[65536];  // 64K RAM
} ram_state_t;

// Forward declaration
struct device_s;

// Optimized I/O handlers (called via callback table)
void ram_init(ram_state_t* ram_dev);

#endif // RAM_H