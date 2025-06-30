#ifndef MOS6567_H
#define MOS6567_H

#include "vicii_common.h"

typedef vicii_common_t mos6567_t;

// Lifecycle and bus attach
void* mos6567_system_create(chip_descriptor_t* desc);
void mos6567_system_destroy(void* chip);
void mos6567_bus_attach(void* chip, void* bus);

// Register I/O
uint8_t mos6567_registers_read(void* chip, uint16_t address);
void mos6567_registers_write(void* chip, uint16_t address, uint8_t value);

// Bank change callback
void mos6567_bank_change(void* chip, uint8_t bank);

// Descriptor for NTSC VIC-II
extern chip_descriptor_t mos6567_descriptor;

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
// GUI function declarations
void mos6567_render_debug_window(void* chip, bool* show_window);
void mos6567_render_settings_window(void* chip, bool* show_window);
#endif

#endif // MOS6567_H