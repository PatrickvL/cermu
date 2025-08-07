#ifndef SYSTEM_H
#define SYSTEM_H

#include "aiemuc.h"
#include "chip.h"

// Unified chip access callback - combines read/write with shared context
typedef struct {
    chip_callback_t read_func;
    chip_callback_t write_func;
    void* context;  // Shared context for both read and write operations
} access_callback_t;

// Generic 8-bit system
typedef struct {
    chip_entry_t chips[16];
    uint8_t chip_count;
} system_8bit_t;

// Function declarations
uint8_t system_chip_register(system_8bit_t* system, void* chip, chip_descriptor_t* desc, uint16_t base, unsigned int size);
void system_chips_destroy(system_8bit_t* system);

#endif // SYSTEM_H
