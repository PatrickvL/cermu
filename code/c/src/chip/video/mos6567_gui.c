#include "mos6567.h"
#include "vicii_gui.h"
#include "../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// MOS6567 VIC-II GUI FUNCTIONS (NTSC)
// ============================================================================

void mos6567_render_debug_window(void* chip, bool* show_window) {
    vicii_render_common_debug_window(chip, show_window, "MOS6567 VIC-II Debug (NTSC)");
}

void mos6567_render_settings_window(void* chip, bool* show_window) {
    vicii_render_common_settings_window(chip, show_window, "MOS6567 VIC-II Settings (NTSC)");
}