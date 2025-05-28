#ifndef SID_H
#define SID_H

#include "c64.h"
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

extern sid_state_t sid;

// SID functions
void sid_init(void);
void sid_cycle(void);

// Forward declaration
struct device_s;

// Optimized I/O handlers (called via callback table)
uint8_t sid_r8(struct device_s* dev);
void sid_w8(struct device_s* dev);

#endif // SID_H