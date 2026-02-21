#include "mos6569.h"
#include "vicii_common.h"
#include <stdlib.h>
#include <stdint.h>

/**
 * PAL VIC-II (MOS6569) lifecycle and bus attach wrappers
 */
vicii_t* mos6569_create() {
    const vicii_chip_config_t* config = vicii_get_default_config(true); // PAL = true
    vicii_t* vicii = vicii_create(config, vicii_memory_bank_change);
    if (vicii) vicii->desc = &mos6569_descriptor;
    return vicii;
}

/**
 * Descriptor for PAL VIC-II
 */
chip_descriptor_t mos6569_descriptor = {
    .description = "MOS6569 VIC-II Video Interface Chip (PAL)",
    .create      = [](chip_descriptor_t*) -> void* { return mos6569_create(); },
    .destroy     = [](void* chip) { vicii_destroy(static_cast<vicii_t*>(chip)); },
    .bus_attach  = vicii_bus_attach
};

// Include GUI implementation
#ifdef IMGUI_VERSION
#include "mos6569_gui.h"
#endif
