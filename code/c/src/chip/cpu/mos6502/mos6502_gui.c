#include "mos6502.h"
#include "../fam65xx/fam65xx_gui.h"
#include "../fam65xx/fam65xx_core.h"
#include "../../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// MOS6502-SPECIFIC GUI FUNCTIONS
// ============================================================================

static const char* mos6502_get_cpu_name(void* cpu_instance) {
    (void)cpu_instance; // Suppress unused parameter warning
    return "MOS 6502";
}

static void mos6502_render_cpu_specific(void* cpu_instance) {
    mos6502_t* cpu = (mos6502_t*)cpu_instance;
    if (!cpu) return;
    
    // Standard MOS6502 has no special hardware to display
    // Could show decimal mode status or other 6502-specific features here
    igText("Standard MOS 6502 Features");
    igSeparator();
    igText("✓ Full decimal mode support");
    igText("✓ All legal and illegal opcodes");
    igText("✓ Compatible with 6502 test suites");
    igSeparator();
}

void mos6502_render_debug_window(void* chip, bool* show_window) {
    mos6502_t* cpu = (mos6502_t*)chip;
    if (!cpu) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "MOS 6502 Debug");
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    // Create GUI configuration for MOS6502
    fam65xx_gui_config_t config = {
        .cpu_type_name = "MOS 6502 (Standard)",
        .has_decimal_mode = true,   // MOS6502 supports full decimal mode
        .has_io_ports = false,      // No I/O ports
        .has_extended_opcodes = false,
        .render_cpu_specific = mos6502_render_cpu_specific
    };
    
    // Use the shared family debug window renderer
    fam65xx_render_debug_window(chip, show_window, &config);
    
    igEnd();
}
