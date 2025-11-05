#include "ram.h"
#include <string.h>
#include <stdlib.h>

void* ram_system_create(chip_descriptor_t* desc) {
    ram_t* ram = (ram_t*)calloc(1, sizeof(ram_t));
    if (!ram) return NULL;
    ram->desc = desc;
    // Note: memory pointer will be set later to point into unified buffer
    ram->memory = NULL;
    return ram;
}

void ram_system_destroy(void* context) {
    free(context);
}

chip_descriptor_t ram_descriptor = {
    .description = "System RAM",
    .create = ram_system_create,
    .destroy = ram_system_destroy,
    .bus_attach = NULL,
    .bank_change = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = ram_render_debug_window,
    .render_settings_window = ram_render_settings_window
#endif
};

// Include GUI implementation
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "ram_gui.h"
#endif
