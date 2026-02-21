#pragma once

#include <stdbool.h>

// Forward declarations
typedef struct ram_s ram_t;

// RAM GUI functions (C++ linkage for consistency)
void ram_render_debug_content(void* chip);
void ram_render_settings_content(void* chip);
void ram_render_layout_content(void* chip);

