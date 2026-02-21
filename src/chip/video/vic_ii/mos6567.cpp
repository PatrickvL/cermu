#include "mos6567.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/*
 * NTSC VIC-II (MOS6567) lifecycle and bus attach wrappers
 */
void* mos6567_system_create(chip_descriptor_t* desc) {
    const vicii_chip_config_t* config = vicii_get_default_config(false); // PAL = false (NTSC)
    vicii_t* vicii = vicii_system_create(desc, config, vicii_memory_bank_change);
    return vicii;
}

/*
 * Descriptor for NTSC VIC-II
 */
chip_descriptor_t mos6567_descriptor = {
    .description = "MOS6567 VIC-II Video Interface Chip (NTSC)",
    .create      = mos6567_system_create,
    .destroy     = vicii_system_destroy,
    .bus_attach  = vicii_bus_attach,
    .bank_change = vicii_memory_bank_change,
#ifdef IMGUI_VERSION
    .render_debug_window = NULL,
    .render_settings_window = NULL
#endif
};

// Include GUI implementation
#ifdef IMGUI_VERSION
#include "mos6567_gui.h"
#endif