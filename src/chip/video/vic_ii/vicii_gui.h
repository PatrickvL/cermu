#pragma once

#include <stdbool.h>

// Common VIC-II GUI rendering functions
void vicii_gui_render_debug_window(void* chip, bool* show_window, const char* window_title);
void vicii_gui_render_settings_window(void* chip, bool* show_window, const char* window_title);
void vicii_gui_render_layout_window(void* chip, bool* show_window, const char* window_title);


