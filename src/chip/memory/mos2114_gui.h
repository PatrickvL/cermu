#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// MOS2114 Color RAM GUI functions
void mos2114_render_debug_window(void* chip, bool* show_window);
void mos2114_render_settings_window(void* chip, bool* show_window);

#ifdef __cplusplus
}
#endif
