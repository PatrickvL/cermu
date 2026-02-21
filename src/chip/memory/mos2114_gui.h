#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// MOS2114 Color RAM GUI functions
void mos2114_render_debug_content(void* chip);
void mos2114_render_settings_content(void* chip);
void mos2114_render_layout_content(void* chip);

#ifdef __cplusplus
}
#endif
