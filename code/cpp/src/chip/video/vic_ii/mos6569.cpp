#include "mos6569.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/**
 * PAL VIC-II (MOS6569) lifecycle and bus attach wrappers
 */
void* mos6569_system_create(chip_descriptor_t* desc) {
    const vicii_chip_config_t* config = vicii_get_default_config(true); // PAL = true
    vicii_t* vicii = vicii_system_create(desc, config, vicii_memory_bank_change);
    return vicii;
}

/**
 * Descriptor for PAL VIC-II
 */
chip_descriptor_t mos6569_descriptor = {
    .description = "MOS6569 VIC-II Video Interface Chip (PAL)",
    .create      = mos6569_system_create,
    .destroy     = vicii_system_destroy,
    .bus_attach  = vicii_bus_attach,
    .bank_change = vicii_memory_bank_change,
#ifdef IMGUI_VERSION
    .render_debug_window = mos6569_render_debug_window,
    .render_settings_window = mos6569_render_settings_window
#endif
};

// Include GUI implementation
#ifdef IMGUI_VERSION
#include "mos6569_gui.h"
#endif
