#pragma once

#include <stdbool.h>

// Forward declarations
typedef struct mos6526_s mos6526_t;

// MOS6526 CIA GUI functions
void mos6526_render_debug_content(void* chip);
void mos6526_render_settings_content(void* chip);
void mos6526_render_layout_content(void* chip);

