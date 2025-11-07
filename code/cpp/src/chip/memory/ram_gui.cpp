#include "ram.h"
#include "../../gui/imgui_interface.h"
#include "../../gui/generic_chip_gui.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <imgui.h>
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
    PIN_LR(layout, 1, A6,   VCC, 18);  // Address 6 / +5V Power
    PIN_LR(layout, 2, A5,   D1,  17);   // Address 5 / Data 1
    PIN_LR(layout, 3, A4,   D2,  16);   // Address 4 / Data 2
    PIN_LR(layout, 4, A3,   D3,  15);   // Address 3 / Data 3
    PIN_LR(layout, 5, A0,   CS,  14);   // Address 0 / Chip Select
    PIN_LR(layout, 6, A1,   WE,  13);   // Address 1 / Write Enable
    PIN_LR(layout, 7, A2,   A9,  12);   // Address 2 / Address 9
    PIN_LR(layout, 8, D0,   A8,  11);   // Data 0    / Address 8
    PIN_LR(layout, 9, VSS,  A7,  10);   // Ground    / Address 7
    
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
    ImGui::Text("RAM Memory");
    ImGui::Separator();
    
    if (ram->desc) {
        ImGui::Text("Size: %s", ram->desc->description);
    } else {
        ImGui::Text("Size: 64KB");
    }
    ImGui::Text("Address Range: $0000-$FFFF");
    
    ImGui::Separator();
    
    static int view_address = 0x0000;
    ImGui::InputInt("View Address", &view_address, 1, 16, 0);
    view_address &= 0xFFFF;
    
    ImGui::Text("Memory at $%04X:", view_address);

    // Show 16 bytes in hex
    for (int row = 0; row < 4; row++) {
        ImGui::Text("%04X: 00 00 00 00", view_address + (row * 4));
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
    ImGui::PushID((int)(uintptr_t)ram);
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", ram->desc->description);
    
    if (!ImGui::Begin(window_title, show_window, 0)) {
        ImGui::End();
        ImGui::PopID();
        return;
    }

    ImGui::Text("RAM Configuration");
    ImGui::Separator();
    
    ImGui::Text("Type: System RAM");
    ImGui::Text("Size: 64KB");

    if (ImGui::Button("Clear All RAM", ImVec2(0, 0))) {
        // Clear RAM
    }
    
    ImGui::SameLine(0, -1.0f);
    if (ImGui::Button("Fill with Pattern", ImVec2(0, 0))) {
        // Fill with pattern
    }

    ImGui::End();
    ImGui::PopID();
}