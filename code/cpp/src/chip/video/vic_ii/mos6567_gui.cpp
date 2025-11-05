#include "mos6567.h"
#include "vicii_gui.h"
#include "../../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// MOS6567 VIC-II GUI FUNCTIONS (NTSC)
// ============================================================================

void mos6567_render_debug_window(void* chip, bool* show_window) {
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii || !vicii->desc) return;

    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", vicii->desc->description);

    vicii_gui_render_debug_window(chip, show_window, window_title);
}

void mos6567_render_settings_window(void* chip, bool* show_window) {
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii || !vicii->desc) return;

    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", vicii->desc->description);

    vicii_gui_render_settings_window(chip, show_window, window_title);
}