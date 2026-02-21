#pragma once

// Legacy header — MOS6526 GUI is now implemented as ChipBase virtual methods.
// These C-linkage wrappers remain for the c64.cpp System8Bit test path.

#ifdef __cplusplus
extern "C" {
#endif

void mos6526_render_debug_content(void* chip);
void mos6526_render_settings_content(void* chip);
void mos6526_render_layout_content(void* chip);

#ifdef __cplusplus
}
#endif

