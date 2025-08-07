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

// RAM memory read function - bus state interface
bus_state_t ram_memory_read(void* context, bus_state_t bus_state) {
    ram_t* ram = (ram_t*)context;
    bus_state.data = ram->memory[bus_state.addr & 0xFFFF];
    return bus_state;
}

// RAM memory write function - bus state interface
bus_state_t ram_memory_write(void* context, bus_state_t bus_state) {
    ram_t* ram = (ram_t*)context;
    ram->memory[bus_state.addr & 0xFFFF] = bus_state.data;
    return bus_state;
}

chip_descriptor_t ram_descriptor = {
    .description = "System RAM",
    .create = ram_system_create,
    .destroy = ram_system_destroy,
    .bus_attach = NULL,
    .read = ram_memory_read,
    .write = ram_memory_write,
    .bank_change = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = ram_render_debug_window,
    .render_settings_window = ram_render_settings_window
#endif
};

// Include GUI implementation
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "ram_gui.c"
#endif
