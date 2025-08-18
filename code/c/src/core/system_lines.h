#ifndef SYSTEM_LINES_H
#define SYSTEM_LINES_H
#include <stdint.h>

// Generic bus state for 16-bit systems (32-bit register value)
typedef struct {
    uint16_t addr;      // Bits 0-15: Address bus
    uint8_t data;       // Bits 16-23: Data bus
    uint8_t lines;      // Bits 24-31: Bus control lines
} bus_state_t;

// Bus control line definitions (shared across most chips)
#define BUS_LINE_IRQ    0 // Interrupt request line (moved to bit 0 for optimization)
#define BUS_LINE_NMI    1 // Non-maskable interrupt line (moved to bit 1 for optimization)
#define BUS_LINE_RW     2 // Read/Write line (1=read, 0=write)
#define BUS_LINE_BA     3 // Bus available line
#define BUS_LINE_AEC    4 // Address enable control line
#define BUS_LINE_RDY    5 // Ready line
#define BUS_LINE_IO_MEM_ACCESS_PENDING 6 // I/O memory access pending line for chip coordination

// Bit masks for easy access
#define BUS_MASK_IRQ        (1 << BUS_LINE_IRQ)
#define BUS_MASK_NMI        (1 << BUS_LINE_NMI)
#define BUS_MASK_RW         (1 << BUS_LINE_RW)
#define BUS_MASK_BA         (1 << BUS_LINE_BA)
#define BUS_MASK_AEC        (1 << BUS_LINE_AEC)
#define BUS_MASK_RDY        (1 << BUS_LINE_RDY)
#define BUS_MASK_IO_MEM_ACCESS_PENDING (1 << BUS_LINE_IO_MEM_ACCESS_PENDING)

// Helper functions for I/O access coordination
static inline void bus_set_io_pending(bus_state_t* state) {
    state->lines |= BUS_MASK_IO_MEM_ACCESS_PENDING;
}

static inline void bus_clear_io_pending(bus_state_t* state) {
    state->lines &= ~BUS_MASK_IO_MEM_ACCESS_PENDING;
}

static inline bool bus_is_io_pending(const bus_state_t* state) {
    return (state->lines & BUS_MASK_IO_MEM_ACCESS_PENDING) != 0;
}

#endif // SYSTEM_LINES_H
