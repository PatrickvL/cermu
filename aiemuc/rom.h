#ifndef ROM_H
#define ROM_H

#include "bus.h"

// ============================================================================
// ROM EMULATION - KERNAL, BASIC, and Character ROM
// ============================================================================

typedef struct {
    device_t device;  // Generic device interface (must be first)
    uint8_t kernal_data[8192];
    uint8_t basic_data[8192];
    uint8_t char_data[4096];
} rom_state_t;

// Forward declaration
struct device_s;

// Optimized I/O handlers (called via callback table)
void rom_init(rom_state_t* rom_dev);
uint8_t basic_r8(struct device_s* dev);
uint8_t char_rom_r8(struct device_s* dev);
uint8_t kernel_r8(struct device_s* dev);

#endif // ROM_H