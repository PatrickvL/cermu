#ifndef CIA_H
#define CIA_H

#include "device.h"
#include "bus.h"

typedef struct {
    device_descriptor_t* desc;
    uint8_t pra, prb, ddra, ddrb;
    uint16_t timer_a, timer_b;
    uint8_t tod_10ths, tod_sec, tod_min, tod_hr;
    uint8_t sdr, icr, cra, crb;
    bool tod_latched;
    uint8_t tod_latch[4];
    bus_interface_t* bus;
} cia_t;

// old

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
    
    // Bus attachment
    bus_state_t* bus;
} cia_state_t;

// Device attachment
void cia_attach_bus(cia_state_t* cia_dev, bus_state_t* bus_state);

#endif // CIA_H