#include "nes6502.h"
#include "../fam65xx/fam65xx_gui.h"
#include "../fam65xx/fam65xx_core.h"
#include "../../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// NES6502-SPECIFIC GUI FUNCTIONS
// ============================================================================

static const char* nes6502_get_cpu_name(void* cpu_instance) {
    (void)cpu_instance; // Suppress unused parameter warning
    return "NES 6502";
}

static void nes6502_render_cpu_specific(void* cpu_instance) {
    nes6502_t* cpu = (nes6502_t*)cpu_instance;
    if (!cpu) return;
    
    // NES 6502 specific features
    igText("NES 6502 Features");
    igSeparator();
    igText("✓ All legal and illegal opcodes");
    igText("✗ Decimal mode disabled (D flag ignored)");
    igText("✓ Compatible with NES test suites");
    igSeparator();
}

void nes6502_render_debug_window(void* chip, bool* show_window) {
    nes6502_t* cpu = (nes6502_t*)chip;
    if (!cpu) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "NES 6502 Debug");
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    // Create GUI configuration for NES6502
    fam65xx_gui_config_t config = {
        .cpu_type_name = "NES 6502 (Nintendo)",
        .has_decimal_mode = false,  // NES6502 does NOT support decimal mode
        .has_io_ports = false,      // No I/O ports
        .has_extended_opcodes = false,
        .render_cpu_specific = nes6502_render_cpu_specific
    };
    
    // Use the shared family debug window renderer
    fam65xx_render_debug_window(chip, show_window, &config);
    
    igEnd();
}
