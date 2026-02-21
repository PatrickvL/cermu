#pragma once

// Legacy GUI wrappers for VIC-II (deprecated — use ChipBase virtual methods)
// These free functions delegate to vicii_s::render_*_content() class methods.

#ifdef __cplusplus
extern "C" {
#endif

void vicii_gui_render_debug_content(void* chip);
void vicii_gui_render_settings_content(void* chip);
void vicii_gui_render_layout_content(void* chip);

#ifdef __cplusplus
}
#endif
