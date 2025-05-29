#ifndef CIA_H
#define CIA_H

#include "device.h"

// ============================================================================
// CIA EMULATION - Timer and I/O chips
// ============================================================================

typedef struct {
    const device_t* device;  // Pointer to device descriptor (must be first)
    
    union {
        uint8_t registers[16];
        struct {
            uint16_t timer_a, timer_b;
            uint16_t timer_a_latch, timer_b_latch;
            uint8_t control_a, control_b;
            uint8_t interrupt_control, interrupt_status;
            uint8_t port_a, port_b;
        };
    };
} cia_state_t;

// Note: Use device_init(), device_cycle(), device_cleanup() instead of wrapper functions

#endif // CIA_H