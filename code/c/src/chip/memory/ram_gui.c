#include "ram.h"
#include "../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// Hardware-accurate RAM chip layout (Generic SRAM - 18-pin DIP)
static void render_ram_chip_layout(ram_t* ram) {
    if (igCollapsingHeader_BoolPtr("Hardware Layout - SRAM", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        igText("Package: 18-pin DIP");
        igText("Static Random Access Memory (SRAM)");
        igSeparator();
        
        // Two-column layout for pins
        igColumns(2, "ram_pinout", true);
        igText("LEFT SIDE:");
        igText("1  - A6 (Address)");
        igText("2  - A5 (Address)");
        igText("3  - A4 (Address)");
        igText("4  - A3 (Address)");
        igText("5  - A0 (Address)");
        igText("6  - A1 (Address)");
        igText("7  - A2 (Address)");
        igText("8  - D0 (Data)");
        igText("9  - VSS (Ground)");
        
        igNextColumn();
        igText("RIGHT SIDE:");
        igText("10 - A7 (Address)");
        igText("11 - A8 (Address)");
        igText("12 - A9 (Address)");
        igText("13 - WE (Write Enable)");
        igText("14 - CS (Chip Select)");
        igText("15 - D3 (Data)");
        igText("16 - D2 (Data)");
        igText("17 - D1 (Data)");
        igText("18 - VCC (+5V)");
        
        igColumns(1, NULL, false);
        igUnindent(16.0f);
    }
}

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

    // Create two-column layout: chip visualization on left, debugging info on right
    igColumns(2, "ram_debug_columns", true);
    
    // Left column: Hardware chip layout
    render_ram_chip_layout(ram);
    
    igNextColumn();
    
    // Right column: RAM information
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

    // Reset to single column at the end
    igColumns(1, NULL, false);
    
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