#include "mos6567.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/*
 * NTSC VIC-II (MOS6567) lifecycle and bus attach wrappers
 */
void* mos6567_system_create(chip_descriptor_t* desc) {
    vicii_common_t* vicii = vicii_common_system_create(desc, mos6567_bank_change);
    if (vicii) {
        vicii->cycles_per_line = MOS6567_CYCLES_PER_LINE;
        vicii->total_lines    = MOS6567_TOTAL_LINES;
    }
    return vicii;
}

void mos6567_system_destroy(void* chip) {
    vicii_common_system_destroy(chip);
}

void mos6567_bus_attach(void* chip, void* bus) {
    vicii_common_bus_attach(chip, bus);
}

/*
 * Register I/O wrappers
 */
uint8_t mos6567_registers_read(void* chip, uint16_t address) {
    return vicii_common_registers_read(chip, address);
}

void mos6567_registers_write(void* chip, uint16_t address, uint8_t value) {
    vicii_common_registers_write(chip, address, value);
}

/*
 * Bank change callback wrapper
 */
void mos6567_bank_change(void* chip, uint8_t bank) {
    vicii_common_bank_change(chip, bank);
}

/*
 * Descriptor for NTSC VIC-II
 */
chip_descriptor_t mos6567_descriptor = {
    .description = "MOS6567 VIC-II Video Interface Chip (NTSC)",
    .create     = mos6567_system_create,
    .destroy    = mos6567_system_destroy,
    .bus_attach = mos6567_bus_attach,
    .read       = mos6567_registers_read,
    .write      = mos6567_registers_write,
    .bank_change= mos6567_bank_change,
    .get_rwcb_context = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = mos6567_render_debug_window,
    .render_settings_window = mos6567_render_settings_window
#endif
};

/*
 * NTSC cycle logic: delegate to common with NTSC timing
 */
void mos6567_cycle(mos6567_t* vicii) {
    vicii_common_cycle((vicii_common_t*)vicii);
}

// Include GUI implementation
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "mos6567_gui.c"
#endif