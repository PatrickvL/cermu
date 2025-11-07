#ifndef MOS2114_GUI_H
#define MOS2114_GUI_H

#include <stdbool.h>

// MOS2114 Color RAM GUI functions
#ifdef __cplusplus
extern "C" {
#endif

void mos2114_render_debug_window(void* chip, bool* show_window);
void mos2114_render_settings_window(void* chip, bool* show_window);

#ifdef __cplusplus
}
#endif

#endif // MOS2114_GUI_H