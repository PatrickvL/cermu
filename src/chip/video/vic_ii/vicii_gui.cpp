#include "vicii_common.h"
#include "../../../core/chip_layout.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#endif
#include <cstdio>
#include <cstring>

// ============================================================================
// COMMON VIC-II GUI RENDERING FUNCTIONS
// ============================================================================

static const char* get_vicii_type_name(vicii_t* vicii) {
    if (vicii && vicii->config && vicii->config->chip_name) {
        return vicii->config->chip_name;
    }
    return "Unknown VIC-II";
}

static const char* get_video_standard(vicii_t* vicii) {
    if (vicii->config->cycles_per_line == 65 && vicii->config->total_lines == 262) {
        return "NTSC 60Hz";
    } else if (vicii->config->cycles_per_line == 63 && vicii->config->total_lines == 312) {
        return "PAL 50Hz";
    }
    return "Unknown";
}

static const char* get_screen_mode(uint8_t cr1, uint8_t cr2) {
    bool ecm = (cr1 & 0x40) != 0;  // Extended Color Mode
    bool bmm = (cr1 & 0x20) != 0;  // Bitmap Mode
    bool mcm = (cr2 & 0x10) != 0;  // Multicolor Mode
    
    if (!ecm && !bmm && !mcm) return "Standard Text";
    if (!ecm && !bmm && mcm)  return "Multicolor Text";
    if (!ecm && bmm && !mcm)  return "Standard Bitmap";
    if (!ecm && bmm && mcm)   return "Multicolor Bitmap";
    if (ecm && !bmm && !mcm)  return "Extended Color Text";
    return "Invalid Mode";
}

// ============================================================================
// MOS6567/6569 VIC-II LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_vicii_layout() {
    // Start with DIP-40 base layout
    ChipLayout layout = create_dip40_layout();
    
    // Clear default pins from create_dip40_layout() and add hardware-accurate VIC-II pins
    layout.left_pins.clear();
    layout.right_pins.clear();
    
    // Update package info for VIC-II
    layout.markings = {
        "MOS6567/6569",              // part_number
        "MOS Technology",            // manufacturer
        {},                     // package_variant
        {},                     // date_code
        {},                     // lot_number
        {},                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate MOS6567/6569 VIC-II pinout (40-pin DIP)
    // Right-hand pins (21-40) are numbered bottom-up, not top-down
    PIN_LR(layout,  1, VDD,     VCC, 40)    // +5V supply
    PIN_LR(layout,  2, PHI0,    SOUND, 39)  // clock in / audio
    PIN_LR(layout,  3, AEC,     _CS, 38)    // addr enable / chip sel
    PIN_LR(layout,  4, BA,      COLOR, 37)  // bus avail / color out
    PIN_LR(layout,  5, RW,      PHI2, 36)   // R/W / clock out
    PIN_LR(layout,  6, _IRQ,    D0, 35)     // interrupt / data lo
    PIN_LR(layout,  7, A6,      D1, 34)     // addr lo
    PIN_LR(layout,  8, A7,      D2, 33)
    PIN_LR(layout,  9, A8,      D3, 32)
    PIN_LR(layout, 10, A9,      D4, 31)
    PIN_LR(layout, 11, A10,     D5, 30)
    PIN_LR(layout, 12, A11,     D6, 29)
    PIN_LR(layout, 13, A12,     D7, 28)     // / data hi
    PIN_LR(layout, 14, A13,     A0, 27)     // addr hi / addr lo
    PIN_LR(layout, 15, _CAS,    A1, 26)     // DRAM col strobe
    PIN_LR(layout, 16, _RAS,    A2, 25)     // DRAM row strobe
    PIN_LR(layout, 17, LUMA,    A3, 24)     // luminance / addr
    PIN_LR(layout, 18, CHROMA,  A4, 23)     // chrominance / addr
    PIN_LR(layout, 19, CSYNC,   A5, 22)     // comp sync / addr hi
    PIN_LR(layout, 20, VSS,     VSS, 21)    // gnd
    
    return layout;
}

// Helper function to get VIC-II pin states for visualization
static std::vector<PinSignalState> get_vicii_pin_states(vicii_t* vicii, const ChipLayout* layout, bus_state_t bus_state) {
    if (!vicii || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto pin_states = populate_pin_states_from_bus(*layout, bus_state);

    // VIC-II specific: IRQ driven by VIC-II (override direction from generic)
    if (vicii->registers.data[0x19] & 0x80) {
        pin_states[5].signal_level = false; // IRQ (pin 6) - active low, asserted
        pin_states[5].drive_direction = true;
        pin_states[5].high_impedance = false;
    }

    // Video output pins (always driven by VIC-II)
    pin_states[16].signal_level = true; // LUMA (pin 17)
    pin_states[16].drive_direction = true;
    pin_states[16].high_impedance = false;
    pin_states[17].signal_level = true; // CHROMA (pin 18)
    pin_states[17].drive_direction = true;
    pin_states[17].high_impedance = false;
    pin_states[18].signal_level = true; // CSYNC (pin 19)
    pin_states[18].drive_direction = true;
    pin_states[18].high_impedance = false;

    return pin_states;
}

// Use global renderer for VIC-II chip visualization
static ChipLayout& get_vicii_layout() {
    static ChipLayout layout = create_vicii_layout();
    return layout;
}

// Class method implementation
void vicii_t::render_debug_content() {
    vicii_t* vicii = this;
    
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
        chip_center.y += 200.0f; // Space for the chip
        
        // Get global renderer and chip layout
        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_vicii_layout();
        
        // Get current pin states from VIC-II
        std::vector<PinSignalState> pin_states = get_vicii_pin_states(vicii, &layout, vicii->bus_snapshot_);
        
        // Render the chip using global renderer
        renderer.render(layout, chip_center, pin_states, get_vicii_type_name(vicii));
    }
    ImGui::EndChild();
    
    ImGui::SameLine(0, 5.0f); // Small gap between columns
    
    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        // VIC-II Information
        ImGui::Text("%s", get_vicii_type_name(vicii));
        ImGui::Separator();
        
        // Basic chip information
        if (ImGui::CollapsingHeader("Chip Information", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Video Standard: %s", get_video_standard(vicii));
            ImGui::Text("Cycles per Line: %d", vicii->config->cycles_per_line);
            ImGui::Text("Total Lines: %d", vicii->config->total_lines);
            ImGui::Text("Current Bank: %d", vicii->memory.bank_base / 0x4000);
        }
        
        // Raster information
        if (ImGui::CollapsingHeader("Raster Information", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Raster Line: %d", vicii->timing.raster_counter);
            ImGui::Text("Raster Cycle: %d", vicii->timing.x_cycle);
            ImGui::Text("Badline Condition: %s", vicii->video_logic.is_bad_line ? "YES" : "NO");
            ImGui::Text("X Coordinate: %d", vicii->timing.x_coordinate);
            
            // Progress bar for raster position
            float raster_progress = (float)vicii->timing.raster_counter / (float)vicii->config->total_lines;
            ImGui::ProgressBar(raster_progress, ImVec2(-1, 0), NULL);
            ImGui::Text("Raster Progress: %.1f%%", raster_progress * 100.0f);
        }
        
        // Control registers
        if (ImGui::CollapsingHeader("Control Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
            uint8_t cr1 = vicii->registers.data[0x11];
            uint8_t cr2 = vicii->registers.data[0x16];
            uint8_t memory_setup = vicii->registers.data[0x18];
            
            ImGui::Text("Control Register 1 ($D011): $%02X", cr1);
            ImGui::Indent(20.0f);
            ImGui::Text("RST8: %d  ECM: %d  BMM: %d  DEN: %d  RSEL: %d",
                   (cr1 >> 7) & 1, (cr1 >> 6) & 1, (cr1 >> 5) & 1,
                   (cr1 >> 4) & 1, (cr1 >> 3) & 1);
            ImGui::Text("YSCROLL: %d", cr1 & 0x07);
            ImGui::Unindent(20.0f);
            
            ImGui::Text("Control Register 2 ($D016): $%02X", cr2);
            ImGui::Indent(20.0f);
            ImGui::Text("RES: %d  MCM: %d  CSEL: %d  XSCROLL: %d",
                   (cr2 >> 5) & 1, (cr2 >> 4) & 1, (cr2 >> 3) & 1, cr2 & 0x07);
            ImGui::Unindent(20.0f);
            
            ImGui::Text("Memory Setup ($D018): $%02X", memory_setup);
            ImGui::Indent(20.0f);
            ImGui::Text("VM: %d  CB: %d", (memory_setup >> 4) & 0x0F, (memory_setup >> 1) & 0x07);
            ImGui::Text("Video Matrix Base: $%04X", ((memory_setup >> 4) & 0x0F) * 0x400);
            ImGui::Text("Character Base: $%04X", ((memory_setup >> 1) & 0x07) * 0x800);
            ImGui::Unindent(20.0f);
            
            ImGui::Text("Current Screen Mode: %s", get_screen_mode(cr1, cr2));
        }
    }
    ImGui::EndChild();
#endif
}

// Class method implementation
void vicii_t::render_settings_content() {
    vicii_t* vicii = this;
    
#ifdef CERMU_HAS_GUI

    ImGui::Text("VIC-II Configuration");
    ImGui::Separator();
    
    ImGui::Text("Chip Type: %s", get_vicii_type_name(vicii));
    ImGui::Text("Video Standard: %s", get_video_standard(vicii));
    ImGui::Text("Timing: %d cycles/line, %d lines/frame", vicii->config->cycles_per_line, vicii->config->total_lines);
    
    ImGui::Separator();
    
    ImGui::Text("Display Settings");
    // Add interactive controls here later if needed
    ImGui::Text("(Settings controls will be added here)");
#endif
}

// ============================================================================
// VIC-II LAYOUT (standalone pinout diagram)
// ============================================================================

// Class method implementation
void vicii_t::render_layout_content() {
    vicii_t* vicii = this;

#ifdef CERMU_HAS_GUI
    const char* chip_name = get_vicii_type_name(vicii);

    ChipLayout& layout = get_vicii_layout();
    std::vector<PinSignalState> pin_states = get_vicii_pin_states(vicii, &layout, vicii->bus_snapshot_);
    render_chip_layout(layout, pin_states, chip_name);
#endif
}
