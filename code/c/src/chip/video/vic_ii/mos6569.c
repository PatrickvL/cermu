#include "mos6569.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/**
 * PAL VIC-II (MOS6569) lifecycle and bus attach wrappers
 */
void* mos6569_system_create(chip_descriptor_t* desc) {
    vicii_common_t* vicii = vicii_common_system_create(desc, mos6569_bank_change);
    if (vicii) {
        vicii->cycles_per_line = MOS6569_CYCLES_PER_LINE;
        vicii->total_lines    = MOS6569_TOTAL_LINES;
    }
    return vicii;
}

void mos6569_system_destroy(void* chip) {
    vicii_common_system_destroy(chip);
}

void mos6569_bus_attach(void* chip, void* bus) {
    vicii_common_bus_attach(chip, bus);
}

/**
 * Register I/O wrappers
 */
uint8_t mos6569_registers_read(void* chip, uint16_t address) {
    return vicii_common_registers_read(chip, address);
}

void mos6569_registers_write(void* chip, uint16_t address, uint8_t value) {
    vicii_common_registers_write(chip, address, value);
}

/**
 * Bank change callback wrapper
 */
void mos6569_bank_change(void* chip, uint8_t bank) {
    vicii_common_bank_change(chip, bank);
}

/**
 * Descriptor for PAL VIC-II
 */
chip_descriptor_t mos6569_descriptor = {
    .description = "MOS6569 VIC-II Video Interface Chip (PAL)",
    .create      = mos6569_system_create,
    .destroy     = mos6569_system_destroy,
    .bus_attach  = mos6569_bus_attach,
    .read        = vicii_common_registers_read, // Not mos6569_registers_read as that's just a forward
    .write       = vicii_common_registers_write, // Note mos6569_registers_write as that's just a forward
    .bank_change = mos6569_bank_change,
    .get_rwcb_context = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = mos6569_render_debug_window,
    .render_settings_window = mos6569_render_settings_window
#endif
};

/**
 * PAL cycle logic: delegate to common with PAL timing constants
 */
void mos6569_cycle(mos6569_t* vicii) {
    vicii_common_cycle((vicii_common_t*)vicii);
}

// Include GUI implementation
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "mos6569_gui.c"
#endif
