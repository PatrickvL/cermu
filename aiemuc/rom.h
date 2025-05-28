#ifndef ROM_H
#define ROM_H

#include "bus.h"

// ============================================================================
// ROM EMULATION - Separate devices for BASIC, KERNAL, and Character ROM
// ============================================================================

// Basic ROM (0xA000-0xBFFF, 8K)
typedef struct {
    device_t device;  // Generic device interface (must be first)
    uint8_t data[8192];
} basic_rom_state_t;

// Kernal ROM (0xE000-0xFFFF, 8K)
typedef struct {
    device_t device;  // Generic device interface (must be first)
    uint8_t data[8192];
} kernal_rom_state_t;

// Character ROM (0xD000-0xDFFF, 4K)
typedef struct {
    device_t device;  // Generic device interface (must be first)
    uint8_t data[4096];
} char_rom_state_t;

// Forward declaration
struct device_s;

// Optimized I/O handlers (called via callback table)
void basic_rom_init(basic_rom_state_t* basic_dev);
void kernal_rom_init(kernal_rom_state_t* kernal_dev);
void char_rom_init(char_rom_state_t* char_dev);

uint8_t basic_rom_r8(struct device_s* dev);
uint8_t kernal_rom_r8(struct device_s* dev);
uint8_t char_rom_r8(struct device_s* dev);

#endif // ROM_H