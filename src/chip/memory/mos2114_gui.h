#pragma once

// Legacy header — MOS2114 GUI is now implemented as ChipBase virtual methods.
// These C-linkage wrappers remain for the c64.cpp System8Bit test path.

#ifdef __cplusplus
extern "C" {
#endif

void mos2114_render_debug_content(void* chip);
void mos2114_render_settings_content(void* chip);
void mos2114_render_layout_content(void* chip);

#ifdef __cplusplus
}
#endif
