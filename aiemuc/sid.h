#ifndef SID_H
#define SID_H

#include "bus.h"

// ============================================================================
// SID EMULATION - Sound chip with envelope generators
// ============================================================================

typedef struct {
    device_t device;  // Generic device interface (must be first)
    
    uint8_t registers[32];
    uint16_t envelope_counter[3];
    uint8_t envelope_state[3];
} sid_state_t;

// Forward declaration
struct device_s;

// SID functions
void sid_init(sid_state_t* sid_dev);
void sid_cycle(sid_state_t* sid_dev);

#endif // SID_H