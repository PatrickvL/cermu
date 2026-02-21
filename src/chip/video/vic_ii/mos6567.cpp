#include "mos6567.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/*
 * NTSC VIC-II (MOS6567) lifecycle and bus attach wrappers
 */
vicii_t* mos6567_create() {
    const vicii_chip_config_t* config = vicii_get_default_config(false); // PAL = false (NTSC)
    vicii_t* vicii = vicii_create(config, vicii_memory_bank_change);
    if (vicii) vicii->desc = &mos6567_descriptor;
    return vicii;
}

/*
 * Descriptor for NTSC VIC-II
 */
chip_descriptor_t mos6567_descriptor = {
    .description = "MOS6567 VIC-II Video Interface Chip (NTSC)",
    .create      = [](chip_descriptor_t*) -> void* { return mos6567_create(); },
    .destroy     = [](void* chip) { vicii_destroy(static_cast<vicii_t*>(chip)); },
    .bus_attach  = vicii_bus_attach,
    .bank_change = vicii_memory_bank_change
};

// Include GUI implementation
#ifdef IMGUI_VERSION
#include "mos6567_gui.h"
#endif