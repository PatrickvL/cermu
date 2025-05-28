#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>

// Bus control line definitions
#define IRQ_LINE    (1 << 0)
#define NMI_LINE    (1 << 1)
#define BA_LINE     (1 << 2)
#define AEC_LINE    (1 << 3)
#define RDY_LINE    (1 << 4)

// Forward declaration
struct device_s;

// Generic device structure - all devices inherit from this
typedef struct {
    uint8_t (*r8)(struct device_s* dev);  // Read 8-bit callback - returns data
    void (*w8)(struct device_s* dev);     // Write 8-bit callback
} device_t;

// Device callback struct with device pointer
typedef struct {
    uint8_t (*read)(struct device_s* dev);   // Returns data with device pointer
    void (*write)(struct device_s* dev);     // Write with device pointer
    struct device_s* device;                 // Pointer to device instance
} device_callbacks_t;

// Generic device instance
typedef struct device_s {
    device_t callbacks;
} device_instance_t;

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