#pragma once

#include <stdbool.h>

// TED 7360 GUI rendering functions
void ted7360_render_debug_window(void* chip, bool* show_window);
void ted7360_render_settings_window(void* chip, bool* show_window);
void ted7360_render_layout_window(void* chip, bool* show_window);
