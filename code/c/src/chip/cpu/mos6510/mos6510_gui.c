#include "mos6510.h"
#include "../mos6502_family/mos6502_family_gui.h"
#include "../mos6502_family/mos6502_family_constants.h"
#include "../../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// Forward declarations
static void mos6510_render_cpu_specific(void* chip);

// ============================================================================
// MOS6510 CPU GUI DEBUG WINDOW
// ============================================================================

void mos6510_render_debug_window(void* chip, bool* show_window) {
    mos6510_t* cpu = (mos6510_t*)chip;
    if (!cpu || !cpu->base.desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", cpu->base.desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }    // Create GUI configuration for MOS6510
    mos6502_family_gui_config_t config;
    config.cpu_type_name = "MOS 6510 (C64)";
    config.has_decimal_mode = false;  // MOS6510 does not support decimal mode
    config.has_io_ports = true;       // MOS6510 has I/O ports
    config.has_extended_opcodes = false;
    config.render_cpu_specific = mos6510_render_cpu_specific;
    
    // Use the shared family debug window renderer
    mos6502_family_render_debug_window(chip, show_window, &config);
}

// ============================================================================
// MOS6510 CPU GUI SETTINGS WINDOW
// ============================================================================

void mos6510_render_settings_window(void* chip, bool* show_window) {
    mos6510_t* cpu = (mos6510_t*)chip;
    if (!cpu || !cpu->base.desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", cpu->base.desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("CPU Configuration");
    igSeparator();
    
    igText("CPU Type: MOS6510");
    igText("Architecture: 8-bit 6502 compatible");
    igText("Clock Speed: ~1 MHz");
    
    igSeparator();
    
    // CPU behavior settings
    static bool decimal_mode_support = true;
    igCheckbox("Decimal Mode Support", &decimal_mode_support);
    
    static bool illegal_opcodes = true;
    igCheckbox("Illegal Opcodes", &illegal_opcodes);

    igSeparator();

    igText("I/O Port Configuration");
    igSeparator();
    
    igText("I/O Port Default Values:");
    
    static int ddr_value = 0x2F;
    static int port_value = 0x37;
    
    igSliderInt("DDR Default", &ddr_value, 0, 255, "$%02X", 0);
    igSliderInt("Port Default", &port_value, 0, 255, "$%02X", 0);
    
    if (igButton("Apply Defaults", (ImVec2){0, 0})) {
        cpu->io_port[0] = (uint8_t)ddr_value;
        cpu->io_port[1] = (uint8_t)port_value;
    }

    igSeparator();

    igText("Debug Options");
    igSeparator();
    
    static bool log_instructions = false;
    static bool break_on_brk = true;
    static bool break_on_jam = true;
    
    igCheckbox("Log Instructions", &log_instructions);
    igCheckbox("Break on BRK", &break_on_brk);
    igCheckbox("Break on JAM", &break_on_jam);
    
    if (igButton("Clear Instruction Log", (ImVec2){0, 0})) {
        // TODO: Implement instruction logging
    }

    igEnd();
}

// ============================================================================
// MOS6510-SPECIFIC GUI FUNCTIONS
// ============================================================================

static void mos6510_render_io_ports(mos6510_t* cpu) {
    if (!cpu) return;
    
    igText("I/O Ports ($0000-$0001)");
    igSeparator();
    
    igText("DDR ($0000): $%02X", cpu->io_port[0]);
    igText("Port ($0001): $%02X", cpu->io_port[1]);
    
    // Show individual port bits using shared helper function
    mos6502_family_render_port_bits(cpu->io_port[1], "Port Bits");
    
    // Show DDR bits as well
    mos6502_family_render_port_bits(cpu->io_port[0], "DDR Bits ");
    
    igSeparator();
}

static const char* mos6510_get_cpu_name(void* cpu_instance) {
    (void)cpu_instance; // Suppress unused parameter warning
    return "MOS 6510";
}

// CPU-specific rendering for MOS6510 (I/O ports)
void mos6510_render_cpu_specific(void* chip) {
    mos6510_t* cpu = (mos6510_t*)chip;
    
    igSeparator();
    igText("I/O Ports:");
    
    // Show the I/O ports with their current values
    igText("Port 0 (DDR): $%02X", cpu->io_port[0]);
    igText("Port 1 (Data): $%02X", cpu->io_port[1]);
    
    // Show interpretation of the port values
    igText("Port Control:");
    igText("  Bit 0 (LORAM): %s", (cpu->io_port[1] & 0x01) ? "1" : "0");
    igText("  Bit 1 (HIRAM): %s", (cpu->io_port[1] & 0x02) ? "1" : "0");  
    igText("  Bit 2 (CHAREN): %s", (cpu->io_port[1] & 0x04) ? "1" : "0");
    igText("  Bit 3 (Cassette Write): %s", (cpu->io_port[1] & 0x08) ? "1" : "0");
    igText("  Bit 4 (Cassette Switch): %s", (cpu->io_port[1] & 0x10) ? "1" : "0");
    igText("  Bit 5 (Cassette Motor): %s", (cpu->io_port[1] & 0x20) ? "1" : "0");
}