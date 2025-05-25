#ifndef BUS_H
#define BUS_H

#include "c64.h"

// ============================================================================
// BUS STATE - Lives in host CPU register for maximum performance
// ============================================================================
typedef union {
    uint64_t raw;
    struct {
        uint16_t address;       // A0-A15
        uint8_t  data;          // D0-D7  
        uint8_t  control_lines; // R/W, IRQ, NMI
        uint8_t  chip_selects;  // Chip select lines
        uint8_t  bus_control;   // BA, AEC, RDY
        uint16_t reserved;
    };
} bus_state_t;

// Global bus state
extern bus_state_t bus_state;

// Bus cycle function
void bus_cycle(void);

#endif // BUS_H