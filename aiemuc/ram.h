#ifndef RAM_H
#define RAM_H

#include "device.h"
#include "bus.h"

typedef struct {
    device_descriptor_t* desc;
    uint8_t memory[65536];
    bus_interface_t* bus;
} ram_t;

// old

// ============================================================================
// RAM EMULATION - 64K system RAM with CPU port handling
// ============================================================================

typedef struct {
    const device_t* device;  // Pointer to device descriptor (must be first)
    uint8_t data[65536];  // 64K RAM
} ram_state_t;

#endif // RAM_H