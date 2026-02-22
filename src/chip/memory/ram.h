#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/system_lines.h"

typedef struct ram_s {
    uint8_t* memory;  // Pointer to memory (will point into unified buffer)
    bool owns_memory;  // True if this chip owns the memory and should free it on destruction
} ram_t;

#ifdef IMGUI_VERSION
// GUI function declarations
void ram_render_debug_window(void* chip, bool* show_window);
void ram_render_settings_window(void* chip, bool* show_window);
#endif

// Typed lifecycle functions
ram_t* ram_create();
void ram_destroy(ram_t* ram);

