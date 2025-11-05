#ifndef MOS6569_GUI_H
#define MOS6569_GUI_H

#include <stdbool.h>

// Forward declarations
typedef struct vicii_s vicii_t;

// MOS6569 VIC-II GUI functions (PAL)
void mos6569_render_debug_window(void* chip, bool* show_window);
void mos6569_render_settings_window(void* chip, bool* show_window);

#endif // MOS6569_GUI_H