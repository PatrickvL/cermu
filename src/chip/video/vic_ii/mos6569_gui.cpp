#include "mos6569.h"
#include "vicii_gui.h"
// Native Dear ImGui C++ - no conditional compilation needed
#include <imgui.h>
#include <stdio.h>

// ============================================================================
// MOS6569 VIC-II GUI FUNCTIONS (PAL)
// ============================================================================

void mos6569_render_debug_window(void* chip, bool* show_window) {
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii || !vicii->desc) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", vicii->desc->description);
    
    vicii_gui_render_debug_window(chip, show_window, window_title);
}

void mos6569_render_settings_window(void* chip, bool* show_window) {
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii || !vicii->desc) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", vicii->desc->description);
    
    vicii_gui_render_settings_window(chip, show_window, window_title);
}