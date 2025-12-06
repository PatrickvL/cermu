#pragma once

#include "../../core/chip.h"

typedef struct ram_s {
    chip_descriptor_t* desc;
    uint8_t* memory;  // Pointer to memory (will point into unified buffer)
    bool owns_memory;  // True if this chip owns the memory and should free it on destruction
} ram_t;

#ifdef IMGUI_VERSION
// GUI function declarations
void ram_render_debug_window(void* chip, bool* show_window);
void ram_render_settings_window(void* chip, bool* show_window);
#endif

extern chip_descriptor_t ram_descriptor;

