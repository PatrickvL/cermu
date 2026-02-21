#pragma once

#include <stdbool.h>

// VIC (MOS 6560/6561) GUI rendering functions
void vic_gui_render_debug_window(void* chip, bool* show_window);
void vic_gui_render_settings_window(void* chip, bool* show_window);
void vic_gui_render_layout_window(void* chip, bool* show_window);
