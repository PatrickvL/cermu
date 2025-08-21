#ifndef SYSTEM_LINES_H
#define SYSTEM_LINES_H
#include <stdint.h>

// Optimized bus state as packed 32-bit value for maximum performance
// Layout: [LINES:31-24][DATA:23-16][ADDR:15-0]
typedef uint32_t bus_state_t;

// Bus field bit layout
#define BUS_ADDR_SHIFT      0
#define BUS_DATA_SHIFT      16  
#define BUS_LINES_SHIFT     24

#define BUS_ADDR_MASK       0x0000FFFF
#define BUS_DATA_MASK       0x00FF0000
#define BUS_LINES_MASK      0xFF000000

// High-performance field access macros
#define BUS_GET_ADDR(state)     ((uint16_t)((state) & BUS_ADDR_MASK))
#define BUS_GET_DATA(state)     ((uint8_t)(((state) & BUS_DATA_MASK) >> BUS_DATA_SHIFT))
#define BUS_GET_LINES(state)    ((uint8_t)(((state) & BUS_LINES_MASK) >> BUS_LINES_SHIFT))

#define BUS_SET_ADDR(state, addr)   ((state) = ((state) & ~BUS_ADDR_MASK) | ((addr) & 0xFFFF))
#define BUS_SET_DATA(state, data)   ((state) = ((state) & ~BUS_DATA_MASK) | (((uint32_t)(data) & 0xFF) << BUS_DATA_SHIFT))
#define BUS_SET_LINES(state, lines) ((state) = ((state) & ~BUS_LINES_MASK) | (((uint32_t)(lines) & 0xFF) << BUS_LINES_SHIFT))

// Optimized constructor macro
#define BUS_STATE(addr, data, lines) \
    (((uint32_t)(addr) & 0xFFFF) | \
     (((uint32_t)(data) & 0xFF) << BUS_DATA_SHIFT) | \
     (((uint32_t)(lines) & 0xFF) << BUS_LINES_SHIFT))

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

// Helper functions for I/O access coordination - updated for packed bus_state_t
static inline void bus_set_io_pending(bus_state_t* state) {
    *state |= (BUS_MASK_IO_MEM_ACCESS_PENDING << BUS_LINES_SHIFT);
}

static inline void bus_clear_io_pending(bus_state_t* state) {
    *state &= ~(BUS_MASK_IO_MEM_ACCESS_PENDING << BUS_LINES_SHIFT);
}

static inline bool bus_is_io_pending(const bus_state_t* state) {
    return (BUS_GET_LINES(*state) & BUS_MASK_IO_MEM_ACCESS_PENDING) != 0;
}

#endif // SYSTEM_LINES_H
