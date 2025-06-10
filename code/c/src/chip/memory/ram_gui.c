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
    (void)chip; // Suppress unused parameter warning
    if (!*show_window) return;
    
    if (!igBegin("RAM Debug", show_window, 0)) {
        igEnd();
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
}

void ram_render_settings_window(void* chip, bool* show_window) {
    (void)chip; // Suppress unused parameter warning
    if (!*show_window) return;
    
    if (!igBegin("RAM Settings", show_window, 0)) {
        igEnd();
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
}