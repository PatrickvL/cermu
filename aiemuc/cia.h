#ifndef CIA_H
#define CIA_H

#include "c64.h"

// ============================================================================
// CIA EMULATION - Timer and I/O chips
// ============================================================================

typedef union {
    uint8_t registers[16];

    struct {
        uint16_t timer_a, timer_b;
        uint16_t timer_a_latch, timer_b_latch;
        uint8_t control_a, control_b;
        uint8_t interrupt_control, interrupt_status;
        uint8_t port_a, port_b;
    };
} cia_state_t;

extern cia_state_t cia1, cia2;

// CIA functions
void cia1_cycle(void);
void cia2_cycle(void);

#endif // CIA_H