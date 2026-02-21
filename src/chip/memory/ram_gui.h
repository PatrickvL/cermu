#pragma once

#include <stdbool.h>

// Forward declarations
typedef struct ram_s ram_t;

// RAM GUI functions (C++ linkage for consistency)
void ram_render_debug_window(void* chip, bool* show_window);
void ram_render_settings_window(void* chip, bool* show_window);
void ram_render_layout_window(void* chip, bool* show_window);

