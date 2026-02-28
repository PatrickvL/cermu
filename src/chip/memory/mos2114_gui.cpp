#include "mos2114.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <cstdio>
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
        {},                     // date_code
        {},                     // lot_number
        {},                     // custom_text
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
    PIN_LR(layout,  1, A6,    VDD, 18)   // addr 6 / +5V
    PIN_LR(layout,  2, A5,    A7,  17)   // addr 5 / addr 7
    PIN_LR(layout,  3, A4,    A8,  16)   // addr 4 / addr 8
    PIN_LR(layout,  4, A3,    A9,  15)   // addr 3 / addr 9
    PIN_LR(layout,  5, A2,    D4,  14)   // addr 2 / data 4
    PIN_LR(layout,  6, A1,    D3,  13)   // addr 1 / data 3
    PIN_LR(layout,  7, A0,    D2,  12)   // addr 0 / data 2
    PIN_LR(layout,  8, _CS,   D1,  11)   // chip sel / data 1
    PIN_LR(layout,  9, VSS,   _WE, 10)   // gnd / write enable
    
    return layout;
}

#ifdef CERMU_HAS_GUI
// Helper function to get MOS2114 pin states for visualization
static std::vector<PinSignalState> get_mos2114_pin_states(MOS2114* mos2114, const ChipLayout* layout, bus_state_t bus_state) {
    if (!mos2114 || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto pin_states = populate_pin_states_from_bus(*layout, bus_state);

    // MOS2114 specific: address pins are inputs (SRAM receives address)
    // The generic function already sets signal_level from bus_state.
    // Override drive_direction for address pins (input to SRAM).
    for (const auto& pin : layout->left_pins) {
        if (pin.get_pin_type() == PinType::ADDRESS && pin.pin_number > 0) {
            pin_states[pin.pin_number - 1].drive_direction = false;
        }
    }
    for (const auto& pin : layout->right_pins) {
        if (pin.get_pin_type() == PinType::ADDRESS && pin.pin_number > 0) {
            pin_states[pin.pin_number - 1].drive_direction = false;
        }
    }

    return pin_states;
}

// Use global renderer for MOS2114 chip visualization
static ChipLayout& get_mos2114_layout() {
    static ChipLayout layout = create_mos2114_layout();
    return layout;
}
#endif

// ============================================================================
// MOS2114 GUI METHODS (ChipBase overrides)
// ============================================================================

void MOS2114::render_debug_content() {

#ifdef CERMU_HAS_GUI
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
        std::vector<PinSignalState> pin_states = get_mos2114_pin_states(this, &layout, this->bus_snapshot_);
        
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
        
        ImGui::Text("Type: MOS2114 Color RAM (1K x 4-bit)");
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
            for (int row = 0; row < 4 && (view_address + row * 16) < 1024; row++) {
                ImGui::Text("%03X: ", view_address + (row * 16));
                ImGui::SameLine();
                for (int col = 0; col < 16 && (view_address + row * 16 + col) < 1024; col++) {
                    uint8_t color_value = memory[view_address + row * 16 + col] & 0x0F;
                    ImGui::SameLine();
                    ImGui::Text("%01X", color_value);
                }
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

void MOS2114::render_settings_content() {

#ifdef CERMU_HAS_GUI

    ImGui::Text("MOS2114 Color RAM Settings");
    ImGui::Separator();
    
    ImGui::Text("Type: MOS2114 Color RAM (1K x 4-bit)");
    ImGui::Text("Size: 1024 x 4-bit");
    ImGui::Text("C64 Usage: Color RAM");

    ImGui::Separator();
    
    if (ImGui::Button("Clear Color RAM", ImVec2(0, 0))) {
        memset(memory, 0, 1024);
    }
    
    ImGui::SameLine(0, 10.0f);
    if (ImGui::Button("Fill with Pattern", ImVec2(0, 0))) {
        // Fill with alternating color pattern
        for (int i = 0; i < 1024; i++) {
            memory[i] = (i % 16) & 0x0F;
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

void MOS2114::render_layout_content() {

#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_mos2114_layout();
    std::vector<PinSignalState> pin_states = get_mos2114_pin_states(this, &layout, this->bus_snapshot_);
    render_chip_layout(layout, pin_states, "MOS2114");
#endif
}