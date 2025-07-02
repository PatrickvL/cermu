#ifndef MOS6569_H
#define MOS6569_H

#include "vicii_common.h"

/**
 * PAL VIC-II (MOS6569) wrapper type.
 */
// MOS6569 PAL timing constants (aliases for consistency)
#define MOS6569_CYCLES_PER_LINE  VICII_PAL_CYCLES_PER_LINE
#define MOS6569_TOTAL_LINES      VICII_PAL_TOTAL_LINES

typedef vicii_common_t mos6569_t;

// Lifecycle and bus attachment
void* mos6569_system_create(chip_descriptor_t* desc);
void  mos6569_system_destroy(void* chip);
void  mos6569_bus_attach(void* chip, void* bus);

// Register I/O
uint8_t mos6569_registers_read(void* chip, uint16_t address);
void    mos6569_registers_write(void* chip, uint16_t address, uint8_t value);

// Bank change callback
void mos6569_bank_change(void* chip, uint8_t bank);

// External descriptor instance
extern chip_descriptor_t mos6569_descriptor;

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
// GUI function declarations
void mos6569_render_debug_window(void* chip, bool* show_window);
void mos6569_render_settings_window(void* chip, bool* show_window);
#endif

#endif // MOS6569_H