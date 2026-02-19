#include "ram.h"
#include "../../gui/imgui_interface.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <stdio.h>
#include <memory>

// ============================================================================
// GENERIC RAM LAYOUT (18-pin DIP for SRAM)
// ============================================================================

inline ChipLayout create_ram_layout() {
    // Start with DIP-18 base layout
    ChipLayout layout = create_dip18_layout();
    
    // Clear default pins from create_dip18_layout() and add hardware-accurate RAM pins
    layout.left_pins.clear();
    layout.right_pins.clear();
    
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
    
    // Hardware-accurate Generic SRAM pinout (18-pin DIP)
    // Right-hand pins (10-18) are numbered bottom-up, not top-down
    PIN_LR(layout,  1, A6,    A7, 18)    // Address 6 / Address 7
    PIN_LR(layout,  2, A5,    A8, 17)    // Address 5 / Address 8
    PIN_LR(layout,  3, A4,    A9, 16)    // Address 4 / Address 9
    PIN_LR(layout,  4, A3,    WE, 15)    // Address 3 / Write Enable
    PIN_LR(layout,  5, A0,    CS, 14)    // Address 0 / Chip Select
    PIN_LR(layout,  6, A1,    D3, 13)    // Address 1 / Data 3
    PIN_LR(layout,  7, A2,    D2, 12)    // Address 2 / Data 2
    PIN_LR(layout,  8, D0,    D1, 11)    // Data 0 / Data 1
    PIN_LR(layout,  9, VSS,   VCC, 10)   // Ground / +5V Power
    
    return layout;
}

// Helper function to get RAM pin states for visualization
static std::vector<PinSignalState> get_ram_pin_states(ram_t* ram, const ChipLayout* layout, bus_state_t bus_state) {
    std::vector<PinSignalState> pin_states;
    if (!ram || !layout) return pin_states;
    
    int total_pins = layout->get_total_pins();
    pin_states.resize(total_pins);
    
    // Initialize all pins as inactive by default
    for (int i = 0; i < total_pins; i++) {
        pin_states[i] = PinSignalState{
            .pin_number = static_cast<uint8_t>(i + 1),
            .signal_level = false,
            .drive_direction = false,
            .signal_value = 0,
            .high_impedance = true,
            .has_pullup = false,
            .has_pulldown = false,
            .signal_valid = true,
            .analog_voltage = 0.0f,
            .is_pwm = false,
            .pwm_duty_cycle = 0.0f
        };
    }
    
    // Set power pins as active
    pin_states[8].signal_level = false;  // VSS (Ground, pin 9)
    pin_states[8].high_impedance = false;
    pin_states[9].signal_level = true;   // VCC (+5V, pin 10)
    pin_states[9].high_impedance = false;
    
    // Set control pins based on bus state (simplified)
    pin_states[13].signal_level = true;  // CS (pin 14) - assume active when accessed
    pin_states[13].drive_direction = false; // Input
    pin_states[13].high_impedance = false;
    pin_states[14].signal_level = false; // WE (pin 15) - simplified
    pin_states[14].drive_direction = false; // Input
    pin_states[14].high_impedance = false;
    
    // Data pins (D0-D3) - show as active during access
    pin_states[7].signal_level = true;   // D0 (pin 8)
    pin_states[7].drive_direction = true; // Output when reading
    pin_states[7].high_impedance = false;
    pin_states[10].signal_level = true;  // D1 (pin 11)
    pin_states[10].drive_direction = true; // Output when reading
    pin_states[10].high_impedance = false;
    pin_states[11].signal_level = true;  // D2 (pin 12)
    pin_states[11].drive_direction = true; // Output when reading
    pin_states[11].high_impedance = false;
    pin_states[12].signal_level = true;  // D3 (pin 13)
    pin_states[12].drive_direction = true; // Output when reading
    pin_states[12].high_impedance = false;
    
    return pin_states;
}

// Use global renderer for RAM chip visualization
static ChipLayout& get_ram_layout() {
    static ChipLayout layout = create_ram_layout();
    return layout;
}

// ============================================================================
// RAM GUI DEBUG WINDOW
// ============================================================================
void ram_render_debug_window(void* chip, bool* show_window) {
    ram_t* ram = (ram_t*)chip;
    if (!ram || !ram->desc) return;
    
    if (!*show_window) return;

#ifdef IMGUI_VERSION
    // Create window title
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", ram->desc->description);
    
    if (!ImGui::Begin(window_title, show_window)) {
        ImGui::End();
        return;
    }

    // Create two-column layout: chip visualization on left, debugging info on right
    ImVec2 window_size = ImGui::GetWindowSize();
    
    // Left column: Chip Visualization (fixed width ~250px)
    ImVec2 chip_viz_size = ImVec2(250.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Chip Visualization");
        ImGui::Separator();
        
        // Calculate chip center for visualization
        ImVec2 chip_center = ImGui::GetCursorScreenPos();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        chip_center.x += content_region.x * 0.5f;
        chip_center.y += 150.0f; // Space for the chip (smaller for 18-pin)
        
        // Get global renderer and chip layout
        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_ram_layout();
        
        // Get current pin states from RAM
        std::vector<PinSignalState> pin_states = get_ram_pin_states(ram, &layout, 0 /* bus_state */);
        
        // Render the chip using global renderer
        renderer.render(layout, chip_center, pin_states, "SRAM");
    }
    ImGui::EndChild();
    
    ImGui::SameLine(0, 5.0f); // Small gap between columns
    
    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        // RAM Information
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
    ImGui::EndChild();

    ImGui::End();
#endif
}

void ram_render_settings_window(void* chip, bool* show_window) {
    ram_t* ram = (ram_t*)chip;
    if (!ram || !ram->desc) return;
    
    if (!*show_window) return;
    
#ifdef IMGUI_VERSION
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
#endif
}