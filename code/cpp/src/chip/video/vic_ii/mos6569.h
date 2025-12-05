#pragma once

#include "vicii_common.h"

/**
 * PAL VIC-II (MOS6569) wrapper type.
 */
// MOS6569 PAL timing constants (aliases for consistency)
#define MOS6569_CYCLES_PER_LINE  VICII_PAL_CYCLES_PER_LINE
#define MOS6569_TOTAL_LINES      VICII_PAL_TOTAL_LINES

typedef vicii_t mos6569_t;

// Lifecycle and bus attachment
void* mos6569_system_create(chip_descriptor_t* desc);

// Register I/O
bus_state_t mos6569_registers_read(void* chip, bus_state_t bus_state);
bus_state_t mos6569_registers_write(void* chip, bus_state_t bus_state);

// External descriptor instance
extern chip_descriptor_t mos6569_descriptor;

#ifdef IMGUI_VERSION
// GUI function declarations
void mos6569_render_debug_window(void* chip, bool* show_window);
void mos6569_render_settings_window(void* chip, bool* show_window);
#endif

