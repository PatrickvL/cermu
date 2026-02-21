#pragma once

#include <stdbool.h>

// VIC (MOS 6560/6561) GUI rendering functions
void vic_gui_render_debug_content(void* chip);
void vic_gui_render_settings_content(void* chip);
void vic_gui_render_layout_content(void* chip);
