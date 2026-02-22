#pragma once

#include "vicii_common.h"

// MOS6567 NTSC timing constants (aliases for consistency)
#define MOS6567_CYCLES_PER_LINE  VICII_NTSC_CYCLES_PER_LINE
#define MOS6567_TOTAL_LINES      VICII_NTSC_TOTAL_LINES

typedef vicii_t mos6567_t;

// Lifecycle and bus attach
vicii_t* mos6567_create();

// Register I/O
bus_state_t mos6567_registers_read(void* chip, bus_state_t bus_state);
bus_state_t mos6567_registers_write(void* chip, bus_state_t bus_state);

// Bank change callback
void mos6567_bank_change(void* chip, uint8_t bank);

