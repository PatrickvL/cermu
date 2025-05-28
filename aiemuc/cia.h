#ifndef CIA_H
#define CIA_H

#include "c64.h"
#include "bus.h"

// ============================================================================
// CIA EMULATION - Timer and I/O chips
// ============================================================================

typedef struct {
    device_t device;  // Generic device interface (must be first)
    
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

extern cia_state_t cia1, cia2;

// CIA functions
void cia1_init(void);
void cia2_init(void);
void cia1_cycle(void);
void cia2_cycle(void);

// Optimized I/O handlers (called via callback table)
uint8_t cia1_r8(void);
void cia1_w8(void);
uint8_t cia2_r8(void);
void cia2_w8(void);

#endif // CIA_H