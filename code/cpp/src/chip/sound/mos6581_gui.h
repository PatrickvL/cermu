#ifndef MOS6581_GUI_H
#define MOS6581_GUI_H

#include <stdbool.h>

// Forward declarations
typedef struct mos6581_s mos6581_t;

// MOS6581 SID GUI functions
void mos6581_render_debug_window(void* chip, bool* show_window);
void mos6581_render_settings_window(void* chip, bool* show_window);

#endif // MOS6581_GUI_H