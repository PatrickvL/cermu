#include "ram.h"
#include "../../gui/cimgui_interface.h"
#include "../../gui/generic_chip_gui.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// GENERIC RAM LAYOUT (18-pin DIP for SRAM)
// ============================================================================

inline ChipLayout create_ram_layout() {
    // Start with DIP-18 base layout
    ChipLayout layout = create_dip18_layout();
    
    // Update package info for RAM
    layout.markings = {
        "SRAM",                      // part_number
        "Generic",                   // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Clear default pins and create generic SRAM pinout (18-pin DIP)
    layout.left_pins.clear();
    layout.right_pins.clear();
    
    // Left side pins (1-9)
    layout.left_pins.push_back(make_pin(1, PinLabel::A6, nullptr));       // Address 6
    layout.left_pins.push_back(make_pin(2, PinLabel::A5, nullptr));       // Address 5
    layout.left_pins.push_back(make_pin(3, PinLabel::A4, nullptr));       // Address 4
    layout.left_pins.push_back(make_pin(4, PinLabel::A3, nullptr));       // Address 3
    layout.left_pins.push_back(make_pin(5, PinLabel::A0, nullptr));       // Address 0
    layout.left_pins.push_back(make_pin(6, PinLabel::A1, nullptr));       // Address 1
    layout.left_pins.push_back(make_pin(7, PinLabel::A2, nullptr));       // Address 2
    layout.left_pins.push_back(make_pin(8, PinLabel::D0, nullptr));       // Data 0
    layout.left_pins.push_back(make_pin(9, PinLabel::VSS, nullptr));      // Ground
    
    // Right side pins (10-18)
    layout.right_pins.push_back(make_pin(18, PinLabel::VCC, nullptr));    // +5V Power
    layout.right_pins.push_back(make_pin(17, PinLabel::D1, nullptr));     // Data 1
    layout.right_pins.push_back(make_pin(16, PinLabel::D2, nullptr));     // Data 2
    layout.right_pins.push_back(make_pin(15, PinLabel::D3, nullptr));     // Data 3
    layout.right_pins.push_back(make_pin(14, PinLabel::CS, nullptr));     // Chip Select
    layout.right_pins.push_back(make_pin(13, PinLabel::WE, nullptr));     // Write Enable
    layout.right_pins.push_back(make_pin(12, PinLabel::A9, nullptr));     // Address 9
    layout.right_pins.push_back(make_pin(11, PinLabel::A8, nullptr));     // Address 8
    layout.right_pins.push_back(make_pin(10, PinLabel::A7, nullptr));     // Address 7
    
    return layout;
}

// Callback functions for generic chip GUI
static ChipLayout get_ram_layout(void* chip) {
    return create_ram_layout();
}

static void get_ram_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, struct PinSignalState* pin_states) {
    ram_t* ram = (ram_t*)chip;
    if (!ram || !layout || !pin_states) return;
    
    int total_pins = 18; // Generic SRAM is 18-pin DIP
    
    // Initialize all pins as inactive by default
    for (int i = 0; i < total_pins; i++) {
        pin_states[i].pin_number = i + 1;
        pin_states[i].signal_level = false;
        pin_states[i].drive_direction = false;
        pin_states[i].signal_value = 0;
        pin_states[i].high_impedance = false;
        pin_states[i].has_pullup = false;
        pin_states[i].has_pulldown = false;
        pin_states[i].signal_valid = true;
        pin_states[i].analog_voltage = 0.0f;
        pin_states[i].is_pwm = false;
        pin_states[i].pwm_duty_cycle = 0.0f;
    }
    
    // Set power pins as active
    pin_states[8].signal_level = false;  // VSS (Ground, pin 9)
    pin_states[17].signal_level = true;  // VCC (+5V, pin 18)
    
    // Set control pins based on bus state (simplified)
    pin_states[13].signal_level = true;  // CS (pin 14) - assume active when accessed
    pin_states[12].signal_level = false; // WE (pin 13) - simplified
    
    // Data pins (D0-D3) - show as active during access
    pin_states[7].signal_level = true;   // D0 (pin 8)
    pin_states[16].signal_level = true;  // D1 (pin 17)
    pin_states[15].signal_level = true;  // D2 (pin 16)
    pin_states[14].signal_level = true;  // D3 (pin 15)
}

static void render_ram_specific_content(void* chip) {
    ram_t* ram = (ram_t*)chip;
    if (!ram) return;
    
    // This replaces the right column content from the original function
    igText("RAM Memory");
    igSeparator();
    
    if (ram->desc) {
        igText("Size: %s", ram->desc->description);
    } else {
        igText("Size: 64KB");
    }
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
}

// ============================================================================
// RAM GUI DEBUG WINDOW
// ============================================================================
void ram_render_debug_window(void* chip, bool* show_window) {
    ram_t* ram = (ram_t*)chip;
    if (!ram || !ram->desc) return;
    
    if (!*show_window) return;
    
    // Create generic chip GUI config
    chip_gui_config_t config = generic_chip_gui_get_default_config("Generic", "SRAM");
    config.get_layout = get_ram_layout;
    config.get_pin_states = get_ram_pin_states;
    
    // Create generic chip GUI instance
    generic_chip_gui_t* gui = generic_chip_gui_create(ram, &config);
    if (!gui) {
        return;
    }
    
    // Create window title
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", ram->desc->description);
    
    // Use generic chip GUI render function
    generic_chip_gui_render_debug_panel(gui, NULL, window_title, show_window, render_ram_specific_content);
    
    // Cleanup
    generic_chip_gui_destroy(gui);
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