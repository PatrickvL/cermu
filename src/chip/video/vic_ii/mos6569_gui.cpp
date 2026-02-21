#include "mos6569.h"
#include "vicii_gui.h"
// Native Dear ImGui C++ - no conditional compilation needed
#include <imgui.h>
#include <stdio.h>

// ============================================================================
// MOS6569 VIC-II GUI FUNCTIONS (PAL)
// These are legacy wrappers. The new system uses ChipBase virtual methods.
// ============================================================================

void mos6569_render_debug_window(void* chip, bool* show_window) {
    // Legacy wrapper — no longer called from new menu system
}

void mos6569_render_settings_window(void* chip, bool* show_window) {
    // Legacy wrapper — no longer called from new menu system
}