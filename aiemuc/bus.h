#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration to avoid circular dependency
typedef struct c64_state_s c64_state_t;

// Bus control line definitions
#define IRQ_LINE    (1 << 0)
#define NMI_LINE    (1 << 1)
#define BA_LINE     (1 << 2)
#define AEC_LINE    (1 << 3)
#define RDY_LINE    (1 << 4)

// ============================================================================
// BUS STATE - Lives in host CPU register for maximum performance
// ============================================================================
typedef struct {
    uint16_t address;       // A0-A15
    uint8_t  data;          // D0-D7
    uint8_t  control_lines; // R/W, IRQ, NMI, BA, AEC, RDY
} bus_state_t;

void bus_init(bus_state_t* bus);

#endif // BUS_H