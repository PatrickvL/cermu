#include "mos2114.h"
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
#include <cstring>  // For memset

// ============================================================================
// MOS2114 1K x 4-bit SRAM LAYOUT (18-pin DIP)
// Hardware-accurate pinout from datasheet
// ============================================================================

inline ChipLayout create_mos2114_layout() {
    // Start with DIP-18 base layout (provides package dimensions only)
    ChipLayout layout = create_dip18_layout();
    
    // Update package info for MOS2114
    layout.markings = {
        "MOS2114",                   // part_number
        "MOS Technology",            // manufacturer
        "1K x 4-bit SRAM",          // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        true,                        // show_package_variant
        false                        // show_date_code
    };

    // Clear default pins from create_dip18_layout() and add hardware-accurate MOS2114 pins
    layout.left_pins.clear();
    layout.right_pins.clear();

    // Hardware-accurate MOS2114 pinout (18-pin DIP) from datasheet
    // Pin layout exactly as specified in the datasheet:
    // Pins 1-7: A6-A0 (Address bits 6 to 0)
    // Pin 8: /CS (Chip Select)
    // Pin 9: GND (Ground)
    // Pin 10: /WE (Write Enable)
    // Pins 11-14: D1-D4 (Data bits 1 to 4)
    // Pins 15-17: A7-A9 (Address bits 7 to 9)
    // Pin 18: Vcc (Supply voltage)
    //
    // Right-hand pins (10-18) are numbered bottom-up as per DIP standard
    PIN_LR(layout,  1, A6,    VDD, 18)   // Address 6 / +5V Power
    PIN_LR(layout,  2, A5,    A7,  17)   // Address 5 / Address 7
    PIN_LR(layout,  3, A4,    A8,  16)   // Address 4 / Address 8
    PIN_LR(layout,  4, A3,    A9,  15)   // Address 3 / Address 9
    PIN_LR(layout,  5, A2,    D4,  14)   // Address 2 / Data 4
    PIN_LR(layout,  6, A1,    D3,  13)   // Address 1 / Data 3
    PIN_LR(layout,  7, A0,    D2,  12)   // Address 0 / Data 2
    PIN_LR(layout,  8, CS,    D1,  11)   // Chip Select (active low) / Data 1
    PIN_LR(layout,  9, VSS,   WE,  10)   // Ground / Write Enable (active low)
    
    return layout;
}

#ifdef IMGUI_VERSION
// Helper function to get MOS2114 pin states for visualization
static std::vector<PinSignalState> get_mos2114_pin_states(mos2114_t* mos2114, const ChipLayout* layout, bus_state_t bus_state) {
    std::vector<PinSignalState> pin_states;
    if (!mos2114 || !layout) return pin_states;
    
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
    pin_states[8].signal_level = false;  // GND (pin 9) - Ground
    pin_states[8].high_impedance = false;
    pin_states[17].signal_level = true;  // Vcc (pin 18) - +5V Power
    pin_states[17].high_impedance = false;
    
    // Set control pins based on current bus state
    pin_states[7].signal_level = true;   // /CS (pin 8) - assume active when accessed
    pin_states[7].drive_direction = false; // Input
    pin_states[7].high_impedance = false;
    pin_states[9].signal_level = false;  // /WE (pin 10) - active low for writes
    pin_states[9].drive_direction = false; // Input
    pin_states[9].high_impedance = false;
    
    // Address pins (A0-A9) - show as active during access
    for (int addr_pin = 0; addr_pin < 10; addr_pin++) {
        int pin_index;
        if (addr_pin <= 6) {
            // A0-A6 are pins 7,6,5,4,3,2,1 (reversed order)
            pin_index = 6 - addr_pin;
        } else {
            // A7-A9 are pins 17,16,15
            pin_index = 10 + (addr_pin - 7);
        }
        pin_states[pin_index].signal_level = true;
        pin_states[pin_index].drive_direction = false; // Input
        pin_states[pin_index].high_impedance = false;
    }
    
    // Data pins (D1-D4) - show as active during access (4-bit wide memory)
    pin_states[10].signal_level = true;  // D1 (pin 11)
    pin_states[10].drive_direction = true; // Output when reading
    pin_states[10].high_impedance = false;
    pin_states[11].signal_level = true;  // D2 (pin 12)
    pin_states[11].drive_direction = true; // Output when reading
    pin_states[11].high_impedance = false;
    pin_states[12].signal_level = true;  // D3 (pin 13)
    pin_states[12].drive_direction = true; // Output when reading
    pin_states[12].high_impedance = false;
    pin_states[13].signal_level = true;  // D4 (pin 14)
    pin_states[13].drive_direction = true; // Output when reading
    pin_states[13].high_impedance = false;
    
    return pin_states;
}

// Use global renderer for MOS2114 chip visualization
static ChipLayout& get_mos2114_layout() {
    static ChipLayout layout = create_mos2114_layout();
    return layout;
}
#endif

// ============================================================================
// MOS2114 GUI FUNCTIONS (always defined for linking, conditionally implemented)
// ============================================================================
extern "C" void mos2114_render_debug_content(void* chip) {
    mos2114_t* mos2114 = (mos2114_t*)chip;
    if (!mos2114 || !mos2114->desc) return;

#ifdef IMGUI_VERSION
    // Create two-column layout: chip visualization on left, debugging info on right
    ImVec2 window_size = ImGui::GetContentRegionAvail();
    
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
        ChipLayout& layout = get_mos2114_layout();
        
        // Get current pin states from MOS2114
        std::vector<PinSignalState> pin_states = get_mos2114_pin_states(mos2114, &layout, 0 /* bus_state */);
        
        // Render the chip using global renderer
        renderer.render(layout, chip_center, pin_states, "MOS2114");
    }
    ImGui::EndChild();
    
    ImGui::SameLine(0, 5.0f); // Small gap between columns
    
    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        // MOS2114 Information
        ImGui::Text("MOS2114 Color RAM");
        ImGui::Separator();
        
        ImGui::Text("Type: %s", mos2114->desc->description);
        ImGui::Text("Capacity: 1024 x 4-bit (1KB)");
        ImGui::Text("Address Range: $D800-$DBFF");
        ImGui::Text("Usage: C64 Color RAM");
        
        ImGui::Separator();
        
        // Memory viewer for Color RAM
        if (ImGui::CollapsingHeader("Color RAM Contents", ImGuiTreeNodeFlags_DefaultOpen)) {
            static int view_address = 0x0000;
            ImGui::InputInt("View Address (offset)", &view_address, 1, 16, 0);
            view_address &= 0x3FF; // Limit to 1K range
            
            ImGui::Text("Color RAM at offset $%03X:", view_address);
            
            // Show 16 nibbles (4-bit values) in a row
            if (mos2114->memory) {
                for (int row = 0; row < 4 && (view_address + row * 16) < 1024; row++) {
                    ImGui::Text("%03X: ", view_address + (row * 16));
                    ImGui::SameLine();
                    for (int col = 0; col < 16 && (view_address + row * 16 + col) < 1024; col++) {
                        uint8_t color_value = mos2114->memory[view_address + row * 16 + col] & 0x0F;
                        ImGui::SameLine();
                        ImGui::Text("%01X", color_value);
                    }
                }
            } else {
                ImGui::Text("Memory not allocated");
            }
        }
        
        // Technical information
        if (ImGui::CollapsingHeader("Technical Details", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Package: 18-pin DIP");
            ImGui::Text("Technology: NMOS Static RAM");
            ImGui::Text("Access Time: ~250ns typical");
            ImGui::Text("Power: +5V single supply");
            ImGui::Separator();
            ImGui::Text("Pin Configuration:");
            ImGui::Text("  Pins 1-7:   A6-A0 (Address)");
            ImGui::Text("  Pin 8:      /CS (Chip Select)");
            ImGui::Text("  Pin 9:      GND (Ground)");
            ImGui::Text("  Pin 10:     /WE (Write Enable)");
            ImGui::Text("  Pins 11-14: D1-D4 (Data)");
            ImGui::Text("  Pins 15-17: A7-A9 (Address)");
            ImGui::Text("  Pin 18:     Vcc (+5V)");
        }
        
        // C64 Integration information
        if (ImGui::CollapsingHeader("C64 Integration", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("PLA Control: _GRW signal gates writes");
            ImGui::Text("Memory Map: $D800-$DBFF (I/O region)");
            ImGui::Text("Function: Color information storage");
            ImGui::Text("Data Width: 4-bit (colors 0-15)");
            ImGui::Text("Upper 4 bits: Floating/undefined");
        }
    }
    ImGui::EndChild();
#endif
}

extern "C" void mos2114_render_settings_content(void* chip) {
    mos2114_t* mos2114 = (mos2114_t*)chip;
    if (!mos2114 || !mos2114->desc) return;

#ifdef IMGUI_VERSION

    ImGui::Text("MOS2114 Color RAM Settings");
    ImGui::Separator();
    
    ImGui::Text("Type: %s", mos2114->desc->description);
    ImGui::Text("Size: 1024 x 4-bit");
    ImGui::Text("C64 Usage: Color RAM");

    ImGui::Separator();
    
    if (ImGui::Button("Clear Color RAM", ImVec2(0, 0))) {
        if (mos2114->memory) {
            memset(mos2114->memory, 0, 1024);
        }
    }
    
    ImGui::SameLine(0, 10.0f);
    if (ImGui::Button("Fill with Pattern", ImVec2(0, 0))) {
        if (mos2114->memory) {
            // Fill with alternating color pattern
            for (int i = 0; i < 1024; i++) {
                mos2114->memory[i] = (i % 16) & 0x0F;
            }
        }
    }
    
    ImGui::Separator();
    ImGui::Text("Color RAM contains 4-bit values (0-15)");
    ImGui::Text("representing C64 text color information.");
#endif
}

// ============================================================================
// MOS2114 LAYOUT (standalone pinout diagram)
// ============================================================================

extern "C" void mos2114_render_layout_content(void* chip) {
    if (!chip) return;

#ifdef IMGUI_VERSION
    mos2114_t* mos2114 = (mos2114_t*)chip;

    ChipLayout& layout = get_mos2114_layout();
    std::vector<PinSignalState> pin_states = get_mos2114_pin_states(mos2114, &layout, 0);
    render_chip_layout(layout, pin_states, "MOS2114");
#endif
}