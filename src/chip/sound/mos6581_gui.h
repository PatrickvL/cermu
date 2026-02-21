#pragma once

// Legacy GUI wrappers for MOS6581 SID (deprecated — use ChipBase virtual methods)
// These free functions delegate to mos6581_s::render_*_content() class methods.

#ifdef __cplusplus
extern "C" {
#endif

void mos6581_render_debug_content(void* chip);
void mos6581_render_settings_content(void* chip);
void mos6581_render_layout_content(void* chip);

#ifdef __cplusplus
}
#endif
