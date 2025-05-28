#ifndef CIA_H
#define CIA_H

#include "bus.h"

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

// Forward declaration
struct device_s;

// CIA functions
void cia1_init(cia_state_t* cia_dev);
void cia2_init(cia_state_t* cia_dev);
void cia1_cycle(cia_state_t* cia_dev);
void cia2_cycle(cia_state_t* cia_dev);

#endif // CIA_H