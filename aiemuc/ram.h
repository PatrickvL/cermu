#ifndef RAM_H
#define RAM_H

#include "c64.h"
#include "bus.h"

// ============================================================================
// RAM EMULATION - 64K system RAM with CPU port handling
// ============================================================================

typedef struct {
    device_t device;  // Generic device interface (must be first)
    uint8_t data[65536];  // 64K RAM
} ram_state_t;

extern ram_state_t ram;

// Optimized I/O handlers (called via callback table)
void ram_init(void);
uint8_t ram_r8(void);
void ram_w8(void);

#endif // RAM_H