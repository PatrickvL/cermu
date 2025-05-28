#ifndef ROM_H
#define ROM_H

#include "c64.h"
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

extern rom_state_t rom;

// Optimized I/O handlers (called via callback table)
void rom_init(void);
uint8_t basic_r8(void);
uint8_t char_rom_r8(void);
uint8_t kernel_r8(void);

#endif // ROM_H