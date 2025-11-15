#include "vicii_common.h"
#include "../../../gui/imgui_interface.h"
#include "../../../core/chip_layout.h"
#include "../../../core/pin_macros.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#endif
#include <stdio.h>
#include <string.h>
#include <memory>

// ============================================================================
// COMMON VIC-II GUI RENDERING FUNCTIONS
// ============================================================================

static const char* get_vicii_type_name(vicii_t* vicii) {
    if (vicii && vicii->desc && vicii->desc->description) {
        return vicii->desc->description;
    }
    return "Unknown VIC-II";
}

static const char* get_video_standard(vicii_t* vicii) {
    if (vicii->timing.cycles_per_line == 65 && vicii->timing.total_lines == 262) {
        return "NTSC 60Hz";
    } else if (vicii->timing.cycles_per_line == 63 && vicii->timing.total_lines == 312) {
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
    
    // Update package info for VIC-II
    layout.markings = {
        "MOS6567/6569",              // part_number
        "MOS Technology",            // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate MOS6567/6569 VIC-II pinout (40-pin DIP)
    // Right-hand pins (21-40) are numbered bottom-up, not top-down
    PIN_LR(layout,  1, VDD,     VCC, 40)    // +5V Power / +5V Power
    PIN_LR(layout,  2, PHI0,    SOUND, 39)  // Clock Input / Sound
    PIN_LR(layout,  3, AEC,     CS, 38)     // Address Enable / Chip Select
    PIN_LR(layout,  4, BA,      COLOR, 37)  // Bus Available / Color Signal
    PIN_LR(layout,  5, RW,      PHI2, 36)   // Read/Write / Clock
    PIN_LR(layout,  6, IRQ,     D0, 35)     // Interrupt / Data 0
    PIN_LR(layout,  7, A6,      D1, 34)     // Address 6 / Data 1
    PIN_LR(layout,  8, A7,      D2, 33)     // Address 7 / Data 2
    PIN_LR(layout,  9, A8,      D3, 32)     // Address 8 / Data 3
    PIN_LR(layout, 10, A9,      D4, 31)     // Address 9 / Data 4
    PIN_LR(layout, 11, A10,     D5, 30)     // Address 10 / Data 5
    PIN_LR(layout, 12, A11,     D6, 29)     // Address 11 / Data 6
    PIN_LR(layout, 13, A12,     D7, 28)     // Address 12 / Data 7
    PIN_LR(layout, 14, A13,     A0, 27)     // Address 13 / Address 0
    PIN_LR(layout, 15, CAS,     A1, 26)     // Column Addr Strobe / Address 1
    PIN_LR(layout, 16, RAS,     A2, 25)     // Row Addr Strobe / Address 2
    PIN_LR(layout, 17, LUMA,    A3, 24)     // Luminance / Address 3
    PIN_LR(layout, 18, CHROMA,  A4, 23)     // Chrominance / Address 4
    PIN_LR(layout, 19, CSYNC,   A5, 22)     // Composite Sync / Address 5
    PIN_LR(layout, 20, VSS,     VSS, 21)    // Ground / Ground
    
    return layout;
}

// Helper function to get VIC-II pin states for visualization
static std::vector<PinSignalState> get_vicii_pin_states(vicii_t* vicii, const ChipLayout* layout, bus_state_t bus_state) {
    std::vector<PinSignalState> pin_states;
    if (!vicii || !layout) return pin_states;
    
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
    pin_states[0].signal_level = true;  // VDD (pin 1)
    pin_states[0].high_impedance = false;
    pin_states[39].signal_level = true; // VCC (pin 40)
    pin_states[39].high_impedance = false;
    pin_states[19].signal_level = false; // VSS (Ground, pin 20)
    pin_states[19].high_impedance = false;
    pin_states[20].signal_level = false; // VSS (Ground, pin 21)
    pin_states[20].high_impedance = false;
    
    // Set IRQ pin state based on VIC-II registers
    if (vicii->registers.data[0x19] & 0x80) {
        pin_states[5].signal_level = false; // IRQ (pin 6) - active low
        pin_states[5].drive_direction = true;
        pin_states[5].high_impedance = false;
    }
    
    // Set video output pins as active
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

void vicii_gui_render_debug_window(void* chip, bool* show_window, const char* window_title) {
    if (!*show_window) return;
    
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii) {
        *show_window = false;
        return;
    }
    
#ifdef IMGUI_VERSION
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
        chip_center.y += 200.0f; // Space for the chip
        
        // Get global renderer and chip layout
        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_vicii_layout();
        
        // Get current pin states from VIC-II
        std::vector<PinSignalState> pin_states = get_vicii_pin_states(vicii, &layout, 0 /* bus_state */);
        
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
            ImGui::Text("Cycles per Line: %d", vicii->timing.cycles_per_line);
            ImGui::Text("Total Lines: %d", vicii->timing.total_lines);
            ImGui::Text("Current Bank: %d", vicii->memory.bank);
        }
        
        // Raster information
        if (ImGui::CollapsingHeader("Raster Information", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Raster Line: %d", vicii->timing.raster_counter);
            ImGui::Text("Raster Cycle: %d", vicii->timing.x_cycle);
            ImGui::Text("Badline Condition: %s", vicii->video_logic.is_bad_line ? "YES" : "NO");
            ImGui::Text("X Coordinate: %d", vicii->timing.x_coordinate);
            
            // Progress bar for raster position
            float raster_progress = (float)vicii->timing.raster_counter / (float)vicii->timing.total_lines;
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

    ImGui::End();
#endif
}

void vicii_gui_render_settings_window(void* chip, bool* show_window, const char* window_title) {
    if (!*show_window) return;
    
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii) {
        *show_window = false;
        return;
    }
    
#ifdef IMGUI_VERSION
    if (!ImGui::Begin(window_title, show_window, 0)) {
        ImGui::End();
        return;
    }

    ImGui::Text("VIC-II Configuration");
    ImGui::Separator();
    
    ImGui::Text("Chip Type: %s", get_vicii_type_name(vicii));
    ImGui::Text("Video Standard: %s", get_video_standard(vicii));
    ImGui::Text("Timing: %d cycles/line, %d lines/frame", vicii->timing.cycles_per_line, vicii->timing.total_lines);
    
    ImGui::Separator();
    
    ImGui::Text("Display Settings");
    // Add interactive controls here later if needed
    ImGui::Text("(Settings controls will be added here)");

    ImGui::End();
#endif
}
