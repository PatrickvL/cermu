#ifndef VICII_GUI_H
#define VICII_GUI_H

#include <stdbool.h>

// Common VIC-II GUI rendering functions
void vicii_gui_render_debug_window(void* chip, bool* show_window, const char* window_title);
void vicii_gui_render_settings_window(void* chip, bool* show_window, const char* window_title);

#endif // VICII_GUI_H
