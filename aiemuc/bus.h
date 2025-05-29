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

// Global bus state
extern bus_state_t bus;

// Global C64 state pointer for cycle counting
extern c64_state_t* c64_system;

// ============================================================================
// PLA EMULATION - Pre-computed chip select maps for each memory mode
// ============================================================================

// PLA functions
void switch_cpu_mode(uint8_t mode);

// Memory access functions
void cpu_read_cycle(uint16_t addr);
void cpu_write_cycle(uint16_t addr, uint8_t value);

void bus_init(bus_state_t* bus);
// Bus cycle function
void bus_cycle(void);

// CPU ready check - hardware accurate BA/RDY handling
#define CPU_READY(cpu_dev) (((cpu_dev)->bus->control_lines & RDY_LINE) != 0)

// Wait for CPU ready with automatic stall handling
#define WAIT_READY_THEN_READ(cpu_dev, addr, label) do { \
    label: \
    if (!CPU_READY(cpu_dev)) { bus_cycle(); goto label; } \
    cpu_read_cycle(addr); \
} while(0)

#define WAIT_READY_THEN_WRITE(cpu_dev, addr, data, label) do { \
    label: \
    if (!CPU_READY(cpu_dev)) { bus_cycle(); goto label; } \
    cpu_write_cycle(addr, data); \
} while(0)

#endif // BUS_H