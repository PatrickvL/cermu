#include "pla.h"
#include "../../gui/imgui_interface.h"
#include "../../gui/chip_visualization.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#include "../../systems/c64/c64_bus.h"
#include "../../systems/c64/c64.h"
#include "../../chip/video/vic_ii/vicii_common.h"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef IMGUI_VERSION
#include <imgui.h>
#endif
#include <stdio.h>
#include <string.h>
#include <memory>

// Helper function to get PLA mode description
static const char* get_pla_mode_cpu_description(uint8_t mode) {
    static char mode_desc[256];
    
    uint8_t loram = mode & 0x01;
    uint8_t hiram = (mode >> 1) & 0x01;
    uint8_t game = (mode >> 4) & 0x01;
    uint8_t exrom = (mode >> 3) & 0x01;
    uint8_t charen = (mode >> 2) & 0x01;
    
    snprintf(mode_desc, sizeof(mode_desc), 
             "#LORAM:%d #HIRAM:%d #EXROM:%d #GAME:%d #CHAREN:%d",
             1 - loram, 1 - hiram, 1 - charen, 1 - exrom, 1 - game);
    
    return mode_desc;
}

// Helper function to get PLA mode description
static const char* get_pla_mode_vicii_description(uint8_t mode, uint16_t bank) {
    static char mode_desc[256];
    
    uint8_t game = (mode >> 4) & 0x01;
    uint8_t exrom = (mode >> 3) & 0x01;
    uint8_t va14 = (bank >> 14) & 0x01;
    
    snprintf(mode_desc, sizeof(mode_desc), 
             "#GAME:%d #EXROM:%d #VA14:%d",
             1 - game, 1 - exrom, 1 - va14);
    
    return mode_desc;
}

// ============================================================================
// C64 PLA LAYOUT (28-pin DIP)
// ============================================================================

inline ChipLayout create_pla_layout() {
    // Start with DIP-28 base layout
    ChipLayout layout = create_dip28_layout();
    
    // Update package info for C64 PLA
    layout.markings = {
        "906114-01",                 // part_number
        "Commodore",                 // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate C64 PLA pinout (28-pin DIP) - Official Specifications
    // Right-hand pins (15-28) are numbered bottom-up, not top-down
    PIN_LR(layout,  1, NC,       VCC, 28);          // FE/NC (Programming)    / +5V Power
    PIN_LR(layout,  2, A13,      A12, 27);          // I7 (A13)              / I8 (A12)
    PIN_LR(layout,  3, A14,      AEC, 26);          // I6 (A14)              / I10 (#AEC)
    PIN_LR(layout,  4, A15,      RW, 25);           // I5 (A15)              / I11 (R/#W)
    PIN_LR(layout,  5, UNKNOWN,  EXROM, 24);        // I4 (#VA14)            / I12 (#EXROM)
    PIN_LR(layout,  6, CHAREN,   GAME, 23);         // I3 (#CHAREN)          / I13 (#GAME)
    PIN_LR(layout,  7, HIRAM,    UNKNOWN, 22);      // I2 (#HIRAM)           / I14 (VA13)
    PIN_LR(layout,  8, LORAM,    UNKNOWN, 21);      // I1 (#LORAM)           / I15 (VA12)
    PIN_LR(layout,  9, CAS,      CS, 20);           // I0 (#CAS)             / #CE (Chip Enable)
    PIN_LR(layout, 10, ROMH,     CASRAM, 19);       // F7 (#ROMH)            / F0 (#CASRAM)
    PIN_LR(layout, 11, ROML,     BASIC, 18);        // F6 (#ROML)            / F1 (#BASIC)
    PIN_LR(layout, 12, IO,       KERNAL, 17);       // F5 (#I/O)             / F2 (#KERNAL)
    PIN_LR(layout, 13, GRW,      CHAROM, 16);       // F4 (GR/#W)            / F3 (#CHAROM)
    PIN_LR(layout, 14, VSS,      CASRAM_PLA, 15);   // VSS (Ground)          / F0 (#CASRAM)
    
    return layout;
}

// ============================================================================
// PLA GUI DEBUG WINDOW
// ============================================================================

// Helper function to get PLA pin states for visualization
static std::vector<PinSignalState> get_pla_pin_states(c64_t* c64, const ChipLayout* layout) {
    std::vector<PinSignalState> pin_states;
    if (!c64 || !layout) return pin_states;
    
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
    
    // Set pin states based on PLA logic
    uint8_t current_mode = c64->bus.pla_banking_mode;
    
    // Power pins are always active
    pin_states[13].signal_level = false;  // VSS (Ground, pin 14)
    pin_states[13].high_impedance = false;
    pin_states[27].signal_level = true;   // VCC (+5V, pin 28)
    pin_states[27].high_impedance = false;
    
    // Control signal pins based on current banking mode
    pin_states[9].signal_level = (current_mode & 0x04) != 0;   // CHAREN (pin 10)
    pin_states[9].high_impedance = false;
    pin_states[10].signal_level = (current_mode & 0x02) != 0;  // HIRAM (pin 11)
    pin_states[10].high_impedance = false;
    pin_states[11].signal_level = (current_mode & 0x01) != 0;  // LORAM (pin 12)
    pin_states[11].high_impedance = false;
    pin_states[22].signal_level = (current_mode & 0x10) != 0;  // GAME (pin 23)
    pin_states[22].high_impedance = false;
    pin_states[23].signal_level = (current_mode & 0x08) != 0;  // EXROM (pin 24)
    pin_states[23].high_impedance = false;
    
    return pin_states;
}

// Get shared chip visualization instance for PLA
static ChipVisualization* get_pla_chip_visualization_instance() {
    static std::unique_ptr<ChipVisualization> chip_viz = nullptr;
    
    // Create chip visualization if not already created
    if (!chip_viz) {
        ChipLayout layout = create_pla_layout();
        chip_viz = std::make_unique<ChipVisualization>(layout);
    }
    
    return chip_viz.get();
}

void pla_render_debug_window(void* chip, bool* show_window) {
    // The chip parameter is expected to be a c64_t* since PLA is part of the C64 bus
    c64_t* c64 = (c64_t*)chip;
    
    if (!c64 || !show_window || !*show_window) return;
        
#ifdef IMGUI_VERSION
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "PLA Debug");
    
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
        
        // Get chip visualization instance and render
        ChipVisualization* chip_viz = get_pla_chip_visualization_instance();
        const ChipLayout* layout = &chip_viz->get_pin_layout();
        
        // Get current pin states from PLA
        std::vector<PinSignalState> pin_states = get_pla_pin_states(c64, layout);
        
        // Render the chip
        chip_viz->render(chip_center, pin_states, "PLA");
        
        ImGui::Separator();
        
        // Visualization Settings Menu
        chip_viz->render_settings_gui();
    }
    ImGui::EndChild();
    
    ImGui::SameLine(0, 5.0f); // Small gap between columns
    
    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        // PLA State Section
        ImGui::Text("Programmable Logic Array - C64 PLA");
        ImGui::Separator();

        // Mode tracking and control
        uint8_t current_mode = c64->bus.pla_banking_mode;
        
        // Static state for PLA debug window
        static bool auto_track_mode = true;
        static int pla_debug_selected_mode = 0;
        
        // Auto-track mode checkbox
        ImGui::Checkbox("Auto-track active mode", &auto_track_mode);
        
        if (auto_track_mode) {
            pla_debug_selected_mode = current_mode;
        }
        
        ImGui::SameLine(0, -1.0f);
        ImGui::Text("Current Mode: %d", current_mode);
        
        // Manual mode selector as active-low toggles in requested order: #LORAM, #HIRAM, #GAME, #EXROM, #CHAREN
        static bool loram_n = false, hiram_n = false, game_n = false, exrom_n = false, charen_n = false;
        // Extract bits from current mode (active-low)
        loram_n = ((pla_debug_selected_mode & 0x01) == 0);
        hiram_n = ((pla_debug_selected_mode & 0x02) == 0);
        game_n  = ((pla_debug_selected_mode & 0x10) == 0);
        exrom_n = ((pla_debug_selected_mode & 0x08) == 0);
        charen_n = ((pla_debug_selected_mode & 0x04) == 0);

        bool changed = false;
        ImGui::Text("Viewing Mode:");
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#LORAM", &loram_n);
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#HIRAM", &hiram_n);
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#GAME", &game_n);
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#EXROM", &exrom_n);
        ImGui::SameLine(0, -1.0f);
        changed |= ImGui::Checkbox("#CHAREN", &charen_n);

        if (changed) {
            // Reconstruct mode from toggles (active-low: 0 = checked)
            pla_debug_selected_mode = 0;
            if (!loram_n)  pla_debug_selected_mode |= 0x01;
            if (!hiram_n)  pla_debug_selected_mode |= 0x02;
            if (!charen_n) pla_debug_selected_mode |= 0x04;
            if (!exrom_n)  pla_debug_selected_mode |= 0x08;
            if (!game_n)   pla_debug_selected_mode |= 0x10;
            auto_track_mode = false;
        }
        
        ImGui::Separator();
        
        // Tab bar for CPU and VIC-II views
        if (ImGui::BeginTabBar("PLA Views", ImGuiTabBarFlags_None)) {
            
            // CPU Memory View Tab
            if (ImGui::BeginTabItem("CPU Memory View", NULL, ImGuiTabItemFlags_None)) {
                // CPU Memory Banking Table
                ImGui::Text("CPU Memory Banking (16 x 4KB banks):");
                ImGui::Text("Mode %d - %s", pla_debug_selected_mode,
                       (pla_debug_selected_mode == current_mode) ? "(ACTIVE)" : "(Preview)");
                ImGui::Text("Configuration: %s", get_pla_mode_cpu_description(pla_debug_selected_mode));
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild();

    ImGui::End();
#endif
}

// ============================================================================
// PLA GUI SETTINGS WINDOW
// ============================================================================

void pla_render_settings_window(void* chip, bool* show_window) {
    c64_t* c64 = (c64_t*)chip;
    if (!c64) return;
    
    if (!*show_window) return;
    
#ifdef IMGUI_VERSION
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "PLA Settings");
    
    if (!ImGui::Begin(window_title, show_window, 0)) {
        ImGui::End();
        return;
    }

    // Show PLA information
    ImGui::Text("Programmable Logic Array - C64 PLA Configuration");
    ImGui::Separator();
    ImGui::Text("Chip Type: Commodore 906114-01 PLA");
    ImGui::Text("Package: 28-pin DIP");

    // PLA State Section
    ImGui::Text("Extended PLA Debug Information");
    ImGui::Separator();
    
    uint8_t current_mode = c64->bus.pla_banking_mode;
    ImGui::Text("Current Banking Mode: %d ($%02X)", current_mode, current_mode);
    ImGui::Text("Configuration: %s", get_pla_mode_cpu_description(current_mode));
    
    ImGui::Separator();
    
    // Control signals breakdown
    ImGui::Text("Control Signals");
    ImGui::Separator();
    
    ImGui::Text("LORAM: %s", (current_mode & 0x01) ? "High" : "Low");
    ImGui::Text("HIRAM: %s", (current_mode & 0x02) ? "High" : "Low");
    ImGui::Text("CHAREN: %s", (current_mode & 0x04) ? "High" : "Low");
    ImGui::Text("EXROM: %s", (current_mode & 0x08) ? "High" : "Low");
    ImGui::Text("GAME: %s", (current_mode & 0x10) ? "High" : "Low");
    
    ImGui::End();
#endif
}
