#pragma once

#include <stdbool.h>

// Forward declarations
typedef struct mos6581_s mos6581_t;

// MOS6581 SID GUI functions
void mos6581_render_debug_content(void* chip);
void mos6581_render_settings_content(void* chip);
void mos6581_render_layout_content(void* chip);

