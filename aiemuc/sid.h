#ifndef SID_H
#define SID_H

#include "device.h"

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