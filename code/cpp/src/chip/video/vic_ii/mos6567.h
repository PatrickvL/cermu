#pragma once

#include "vicii_common.h"

// MOS6567 NTSC timing constants (aliases for consistency)
#define MOS6567_CYCLES_PER_LINE  VICII_NTSC_CYCLES_PER_LINE
#define MOS6567_TOTAL_LINES      VICII_NTSC_TOTAL_LINES

typedef vicii_t mos6567_t;

// Lifecycle and bus attach
void* mos6567_system_create(chip_descriptor_t* desc);
void mos6567_system_destroy(void* chip);
void mos6567_bus_attach(void* chip, void* bus);

// Register I/O
bus_state_t mos6567_registers_read(void* chip, bus_state_t bus_state);
bus_state_t mos6567_registers_write(void* chip, bus_state_t bus_state);

// Bank change callback
void mos6567_bank_change(void* chip, uint8_t bank);

// Descriptor for NTSC VIC-II
extern chip_descriptor_t mos6567_descriptor;

#ifdef IMGUI_VERSION
// GUI function declarations
void mos6567_render_debug_window(void* chip, bool* show_window);
void mos6567_render_settings_window(void* chip, bool* show_window);
#endif

