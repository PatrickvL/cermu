#ifndef SID_H
#define SID_H

#include "device.h"
#include "bus.h"

typedef struct {
    device_descriptor_t* desc;
    uint8_t registers[29];
    uint8_t pot_x, pot_y;
    uint8_t osc3, env3;
    bus_interface_t* bus;
} sid_t;

// old

// ============================================================================
// SID EMULATION - Sound chip with envelope generators
// ============================================================================

typedef struct {
    const device_t* device;  // Pointer to device descriptor (must be first)
    
    uint8_t registers[32];
    uint16_t envelope_counter[3];
    uint8_t envelope_state[3];
} sid_state_t;

#endif // SID_H