#ifndef MOS6567_GUI_H
#define MOS6567_GUI_H

#include <stdbool.h>

// Forward declarations
typedef struct vicii_s vicii_t;

// MOS6567 VIC-II GUI functions (NTSC)
void mos6567_render_debug_window(void* chip, bool* show_window);
void mos6567_render_settings_window(void* chip, bool* show_window);

#endif // MOS6567_GUI_H