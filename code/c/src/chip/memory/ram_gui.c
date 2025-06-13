#include "ram.h"
#include "../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// RAM GUI DEBUG WINDOW
// ============================================================================
void ram_render_debug_window(void* chip, bool* show_window) {
    ram_t* ram = (ram_t*)chip;
    if (!ram || !ram->desc) return;
    
    if (!*show_window) return;
    
    // Push unique ID to prevent conflicts between multiple RAM instances
    igPushID_Int((int)(uintptr_t)ram);
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", ram->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        igPopID();
        return;
    }

    igText("RAM Memory");
    igSeparator();
    
    igText("Size: 64KB");
    igText("Address Range: $0000-$FFFF");
    
    igSeparator();
    
    static int view_address = 0x0000;
    igInputInt("View Address", &view_address, 1, 16, 0);
    view_address &= 0xFFFF;
    
    igText("Memory at $%04X:", view_address);

    // Show 16 bytes in hex
    for (int row = 0; row < 4; row++) {
        igText("%04X: 00 00 00 00", view_address + (row * 4));
    }

    igEnd();
    igPopID();
}
void ram_render_settings_window(void* chip, bool* show_window) {
    ram_t* ram = (ram_t*)chip;
    if (!ram || !ram->desc) return;
    
    if (!*show_window) return;
    
    // Push unique ID to prevent conflicts between multiple RAM instances
    igPushID_Int((int)(uintptr_t)ram);
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", ram->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        igPopID();
        return;
    }

    igText("RAM Configuration");
    igSeparator();
    
    igText("Type: System RAM");
    igText("Size: 64KB");

    if (igButton("Clear All RAM", (ImVec2){0, 0})) {
        // Clear RAM
    }
    
    igSameLine(0, -1.0f);
    if (igButton("Fill with Pattern", (ImVec2){0, 0})) {
        // Fill with pattern
    }

    igEnd();
    igPopID();
}