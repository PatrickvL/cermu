#ifndef VICII_COMMON_H
#define VICII_COMMON_H

#include "../../core/chip.h"
#include <stdint.h>
#include <stdbool.h>

// Generic VIC-II state (shared between PAL and NTSC)
typedef struct {
    chip_descriptor_t* desc;
    uint8_t registers[47];
    uint8_t raster_cycle;
    uint16_t raster_line;
    bool badline_condition;
    bool prev_ba;
    uint8_t collision_sprite;
    uint8_t collision_bg;
    uint8_t bank;
    void* bus;  // opaque pointer to c64_bus_t
    void (*bank_change)(void* context, uint8_t bank);
} vicii_common_t;

// Factory and lifecycle
void* vicii_common_system_create(chip_descriptor_t* desc, void (*bank_change)(void*, uint8_t));
void vicii_common_system_destroy(void* chip);

// Bus attachment
void vicii_common_bus_attach(void* chip, void* bus);

// Register I/O
uint8_t vicii_common_registers_read(void* chip, uint16_t address);
void vicii_common_registers_write(void* chip, uint16_t address, uint8_t value);

// Bank change callback
void vicii_common_bank_change(void* chip, uint8_t bank);

// Cycle logic, parameterized by cycles_per_line and total_lines
void vicii_common_cycle(vicii_common_t* vicii, uint8_t cycles_per_line, uint16_t total_lines);

#endif // VICII_COMMON_H