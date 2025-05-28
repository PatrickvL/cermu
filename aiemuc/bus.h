#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include "c64.h"

// Device callback function pointers
typedef void (*device_read_callback_t)(void);
typedef void (*device_write_callback_t)(void);

// Device callback struct
typedef struct {
    device_read_callback_t read;
    device_write_callback_t write;
} device_callbacks_t;

// ============================================================================
// BUS STATE - Lives in host CPU register for maximum performance
// ============================================================================
typedef struct {
    uint16_t address;       // A0-A15
    uint8_t  data;          // D0-D7
    uint8_t  control_lines; // R/W, IRQ, NMI, BA, AEC, RDY
    uint64_t total_cycles;  // Total cycles executed
} bus_state_t;

// Global bus state
extern bus_state_t bus;

// ============================================================================
// PLA EMULATION - Pre-computed chip select maps for each memory mode
// ============================================================================

// PLA functions
void switch_cpu_mode(uint8_t mode);
void generate_pla_maps(void);

// Memory access functions
void cpu_read_cycle(uint16_t addr);
void cpu_write_cycle(uint16_t addr, uint8_t value);

void bus_init(void);
// Bus cycle function
void bus_cycle(void);

// CPU ready check - hardware accurate BA/RDY handling
#define CPU_READY() ((bus.control_lines & RDY_LINE) != 0)

// Wait for CPU ready with automatic stall handling
#define WAIT_READY_THEN_READ(addr, label) do { \
    label: \
    if (!CPU_READY()) { bus_cycle(); goto label; } \
    cpu_read_cycle(addr); \
} while(0)

#define WAIT_READY_THEN_WRITE(addr, data, label) do { \
    label: \
    if (!CPU_READY()) { bus_cycle(); goto label; } \
    cpu_write_cycle(addr, data); \
} while(0)

#endif // BUS_H