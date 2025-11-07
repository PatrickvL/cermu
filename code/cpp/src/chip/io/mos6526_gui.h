#ifndef MOS6526_GUI_H
#define MOS6526_GUI_H

#include <stdbool.h>

// Forward declarations
typedef struct mos6526_s mos6526_t;

// MOS6526 CIA GUI functions
void mos6526_render_debug_window(void* chip, bool* show_window);
void mos6526_render_settings_window(void* chip, bool* show_window);

#endif // MOS6526_GUI_H