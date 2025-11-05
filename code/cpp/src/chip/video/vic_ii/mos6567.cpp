#include "mos6567.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/*
 * NTSC VIC-II (MOS6567) lifecycle and bus attach wrappers
 */
void* mos6567_system_create(chip_descriptor_t* desc) {
    const vicii_chip_config_t* config = vicii_get_default_config(false); // PAL = false (NTSC)
    vicii_t* vicii = vicii_system_create(desc, config, mos6567_bank_change);
    return vicii;
}

void mos6567_system_destroy(void* chip) {
    vicii_system_destroy(chip);
}

void mos6567_bus_attach(void* chip, void* bus) {
    vicii_bus_attach(chip, bus);
}

/*
 * Register I/O wrappers
 */
// Removed unused mos6567_registers_read/write wrapper functions
// These were just forwarding to vicii_registers_read/write which are used directly in descriptors

/*
 * Bank change callback wrapper
 */
void mos6567_bank_change(void* chip, uint8_t bank) {
    vicii_bank_change(chip, bank);
}

/*
 * Descriptor for NTSC VIC-II
 */
chip_descriptor_t mos6567_descriptor = {
    .description = "MOS6567 VIC-II Video Interface Chip (NTSC)",
    .create      = mos6567_system_create,
    .destroy     = mos6567_system_destroy,
    .bus_attach  = mos6567_bus_attach,
    .bank_change = mos6567_bank_change,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = mos6567_render_debug_window,
    .render_settings_window = mos6567_render_settings_window
#endif
};

// Include GUI implementation
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "mos6567_gui.h"
#endif