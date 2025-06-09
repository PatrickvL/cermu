#ifndef MOS6597_H
#define MOS6597_H

#include "vicii_common.h"

typedef vicii_common_t mos6597_t;

// Lifecycle and bus attach
void* mos6597_system_create(chip_descriptor_t* desc);
void mos6597_system_destroy(void* chip);
void mos6597_bus_attach(void* chip, void* bus);

// Register I/O
uint8_t mos6597_registers_read(void* chip, uint16_t address);
void mos6597_registers_write(void* chip, uint16_t address, uint8_t value);

// Bank change callback
void mos6597_bank_change(void* chip, uint8_t bank);

// Descriptor for NTSC VIC-II
extern chip_descriptor_t mos6597_descriptor;

// Timing constants (NTSC stub)
#define MOS6597_CYCLES_PER_LINE 65
#define MOS6597_TOTAL_LINES    262

/**
 * NTSC cycle logic: delegate to common with NTSC timing
 */
void mos6597_cycle(mos6597_t* vicii);

#endif // MOS6597_H