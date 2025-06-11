#include "mos6581.h"
#include "../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// MOS6581 SID GUI DEBUG WINDOW
// ============================================================================
void mos6581_render_debug_window(void* chip, bool* show_window) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !sid->desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", sid->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("MOS 6581 SID (Sound Interface Device)");
    igSeparator();
    
    igText("Voice 1");
    igText("  Frequency: $0000");
    igText("  Pulse Width: $000");
    igText("  Control: $00");
    igText("  Attack/Decay: $00");
    igText("  Sustain/Release: $00");
    
    igSeparator();
    
    igText("Voice 2");
    igText("  Frequency: $0000");
    igText("  Pulse Width: $000");
    igText("  Control: $00");
    igText("  Attack/Decay: $00");
    igText("  Sustain/Release: $00");
    
    igSeparator();
    
    igText("Voice 3");
    igText("  Frequency: $0000");
    igText("  Pulse Width: $000");
    igText("  Control: $00");
    igText("  Attack/Decay: $00");
    igText("  Sustain/Release: $00");
    
    igSeparator();
    
    igText("Filter & Volume");
    igText("  Filter Cutoff: $0000");
    igText("  Filter Resonance: $00");
    igText("  Filter Mode: $00");
    igText("  Master Volume: $00");

    igEnd();
}

// ============================================================================
// MOS6581 SID GUI SETTINGS WINDOW
// ============================================================================
void mos6581_render_settings_window(void* chip, bool* show_window) {
    mos6581_t* sid = (mos6581_t*)chip;
    if (!sid || !sid->desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", sid->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("SID Configuration");
    igSeparator();
    
    igText("Chip Type: MOS6581 SID");
    igText("Base Address: $D400-$D7FF");
    
    igSeparator();
    
    static bool sound_enabled = true;
    igCheckbox("Sound Enabled", &sound_enabled);
    
    static float master_volume = 1.0f;
    igSliderFloat("Master Volume", &master_volume, 0.0f, 1.0f, "%.2f", 0);
    
    igSeparator();
    
    igText("Filter Settings");
    static bool filter_enabled = true;
    igCheckbox("Filter Enabled", &filter_enabled);
    
    if (igButton("Reset SID", (ImVec2){0, 0})) {
        // Reset SID registers
    }

    igEnd();
}