#include "pla.h"
#include "../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// PLA GUI DEBUG WINDOW
// ============================================================================

void pla_render_debug_window(void* chip, bool* show_window) {
    if (!*show_window) return;
    
    if (!igBegin("PLA Debug", show_window, 0)) {
        igEnd();
        return;
    }

    igText("Programmable Logic Array");
    igSeparator();
    
    igText("Memory Banking Configuration");
    igText("Current Bank: 0");
    igText("BASIC ROM: Enabled");
    igText("KERNAL ROM: Enabled");
    igText("Character ROM: Enabled");
    igText("I/O Area: Enabled");
    
    igSeparator();
    
    igText("Memory Map");
    igText("$0000-$9FFF: RAM");
    igText("$A000-$BFFF: BASIC ROM");
    igText("$C000-$CFFF: RAM");
    igText("$D000-$DFFF: I/O");
    igText("$E000-$FFFF: KERNAL ROM");

    igEnd();
}

void pla_render_settings_window(void* chip, bool* show_window) {
    if (!*show_window) return;
    
    if (!igBegin("PLA Settings", show_window, 0)) {
        igEnd();
        return;
    }

    igText("PLA Configuration");
    igSeparator();
    
    igText("Type: C64 Memory Banking PLA");
    
    static bool basic_enabled = true;
    igCheckbox("BASIC ROM Enabled", &basic_enabled);
    
    static bool kernal_enabled = true;
    igCheckbox("KERNAL ROM Enabled", &kernal_enabled);
    
    static bool char_enabled = true;
    igCheckbox("Character ROM Enabled", &char_enabled);
    
    static bool io_enabled = true;
    igCheckbox("I/O Area Enabled", &io_enabled);

    igEnd();
}