#ifndef RAM_H
#define RAM_H

#include "../../core/chip.h"

typedef struct ram_s {
    chip_descriptor_t* desc;
    uint8_t* memory;  // Pointer to memory (will point into unified buffer)
} ram_t;

// RAM access functions - bus state interface
bus_state_t ram_memory_read(void* context, bus_state_t bus_state);
bus_state_t ram_memory_write(void* context, bus_state_t bus_state);

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
// GUI function declarations
void ram_render_debug_window(void* chip, bool* show_window);
void ram_render_settings_window(void* chip, bool* show_window);
#endif

extern chip_descriptor_t ram_descriptor;

#endif // RAM_H