#include "mos6567.h"
#include "vicii_gui.h"
// Native Dear ImGui C++ - no conditional compilation needed
#include <imgui.h>
#include <stdio.h>

// ============================================================================
// MOS6567 VIC-II GUI FUNCTIONS (NTSC)
// These are legacy wrappers. The new system uses vicii_gui_render_*_content()
// directly via CChipAdapter lambdas.
// ============================================================================

void mos6567_render_debug_window(void* chip, bool* show_window) {
    // Legacy wrapper — no longer called from new menu system
}

void mos6567_render_settings_window(void* chip, bool* show_window) {
    // Legacy wrapper — no longer called from new menu system
}