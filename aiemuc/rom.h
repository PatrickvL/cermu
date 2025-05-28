#ifndef ROM_H
#define ROM_H

#include "bus.h"

// ============================================================================
// ROM EMULATION - Generic ROM device with configurable size
// ============================================================================

#define MAX_ROM_SIZE 8192  // Maximum ROM size (8K)

typedef struct {
    const device_t* device;  // Pointer to device descriptor (must be first)
    uint8_t* data;          // Pointer to ROM data (dynamically allocated)
    uint16_t size;          // Size of ROM in bytes
    uint16_t base_address;  // Base address for address calculation
} rom_state_t;

// Forward declaration
struct device_s;

// ROM initialization with configurable size and base address
void rom_init(rom_state_t* rom_dev, uint16_t size, uint16_t base_address);

// ROM cleanup (free allocated memory)
void rom_cleanup(rom_state_t* rom_dev);

#endif // ROM_H