#include "mos6510.h"
#include "../fam65xx/fam65xx_gui.h"
#include "../fam65xx/fam65xx_core.h"
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
    fam65xx_gui_config_t config;
    config.cpu_type_name = "MOS 6510 (C64)";
    config.has_decimal_mode = false;  // MOS6510 does not support decimal mode
    config.has_io_ports = true;       // MOS6510 has I/O ports
    config.has_extended_opcodes = false;
    config.render_cpu_specific = mos6510_render_cpu_specific;
    
    // Use the shared family debug window renderer
    fam65xx_render_debug_window(chip, show_window, &config);
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
        bus_state_t bus_state_ddr = { .addr = 0, .data = (uint8_t)ddr_value, .lines = 0 };
        bus_state_t bus_state_port = { .addr = 1, .data = (uint8_t)port_value, .lines = 0 };
        mos6510_handle_io_write(cpu, bus_state_ddr);
        mos6510_handle_io_write(cpu, bus_state_port);
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

// Removed unused static functions:
// - mos6510_render_io_ports (unused)
// - mos6510_get_cpu_name (unused)

// CPU-specific rendering for MOS6510 (I/O ports)
void mos6510_render_cpu_specific(void* chip) {
    mos6510_t* cpu = (mos6510_t*)chip;
    
    igSeparator();
    igText("I/O Ports:");
    
    // Show the I/O ports with their current values
    bus_state_t bus_state_ddr = { .addr = 0, .data = 0xFF, .lines = BUS_MASK_RW };
    bus_state_t bus_state_port = { .addr = 1, .data = 0xFF, .lines = BUS_MASK_RW };
    bus_state_ddr = mos6510_handle_io_read(cpu, bus_state_ddr);
    bus_state_port = mos6510_handle_io_read(cpu, bus_state_port);
    uint8_t ddr_val = bus_state_ddr.data;
    uint8_t port_val = bus_state_port.data;
    
    igText("Port 0 (DDR): $%02X", ddr_val);
    igText("Port 1 (Data): $%02X", port_val);
    
    // Show interpretation of the port values
    igText("Port Control:");
    igText("  Bit 0 (LORAM): %s", (port_val & 0x01) ? "1" : "0");
    igText("  Bit 1 (HIRAM): %s", (port_val & 0x02) ? "1" : "0");
    igText("  Bit 2 (CHAREN): %s", (port_val & 0x04) ? "1" : "0");
    igText("  Bit 3 (Cassette Write): %s", (port_val & 0x08) ? "1" : "0");
    igText("  Bit 4 (Cassette Switch): %s", (port_val & 0x10) ? "1" : "0");
    igText("  Bit 5 (Cassette Motor): %s", (port_val & 0x20) ? "1" : "0");
}
