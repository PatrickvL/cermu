#include "vicii_common.h"
#include "../../../gui/imgui_interface.h"
#include "../../../gui/generic_chip_gui.h"
#include "../../../core/chip_layout.h"
#include "../../../core/pin_macros.h"
#ifndef IMGUI_VERSION
#define IMGUI_VERSION
#endif
#include <imgui.h>
#include <stdio.h>
#include <string.h>

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
    PIN_LR(layout, 1, VDD,      VSS, 21)    // +5V Power / Ground
    PIN_LR(layout, 2, PHI0,     A5, 22)     // Clock Input / Address 5
    PIN_LR(layout, 3, AEC,      A4, 23)     // Address Enable / Address 4
    PIN_LR(layout, 4, BA,       A3, 24)     // Bus Available / Address 3
    PIN_LR(layout, 5, RW,       A2, 25)     // Read/Write / Address 2
    PIN_LR(layout, 6, IRQ,      A1, 26)     // Interrupt / Address 1
    PIN_LR(layout, 7, A6,       A0, 27)     // Address 6 / Address 0
    PIN_LR(layout, 8, A7,       D7, 28)     // Address 7 / Data 7
    PIN_LR(layout, 9, A8,       D6, 29)     // Address 8 / Data 6
    PIN_LR(layout, 10, A9,      D5, 30)     // Address 9 / Data 5
    PIN_LR(layout, 11, A10,     D4, 31)     // Address 10 / Data 4
    PIN_LR(layout, 12, A11,     D3, 32)     // Address 11 / Data 3
    PIN_LR(layout, 13, A12,     D2, 33)     // Address 12 / Data 2
    PIN_LR(layout, 14, A13,     D1, 34)     // Address 13 / Data 1
    PIN_LR(layout, 15, CAS,     D0, 35)     // Column Addr Strobe / Data 0
    PIN_LR(layout, 16, RAS,     PHI2, 36)   // Row Addr Strobe / Clock
    PIN_LR(layout, 17, LUMA,    COLOR, 37)  // Luminance / Color Signal
    PIN_LR(layout, 18, CHROMA,  CS, 38)     // Chrominance / Chip Select
    PIN_LR(layout, 19, CSYNC,   SOUND, 39)  // Composite Sync / Sound
    PIN_LR(layout, 20, VSS,     VCC, 40)    // Ground / +5V Power
    
    return layout;
}

// Callback functions for generic chip GUI
static ChipLayout get_vicii_layout(void* chip) {
    return create_vicii_layout();
}

static void get_vicii_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, struct PinSignalState* pin_states) {
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii || !layout || !pin_states) return;
    
    // For now, implement basic pin state logic
    // This would be enhanced with actual VIC-II state reading
    int total_pins = 40; // VIC-II is 40-pin DIP
    
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
    pin_states[0].signal_level = true;  // VDD (pin 1)
    pin_states[39].signal_level = true; // VCC (pin 40)
    
    // Set IRQ pin state based on VIC-II registers
    if (vicii->registers.data && (vicii->registers.data[0x19] & 0x80)) {
        pin_states[5].signal_level = true; // IRQ (pin 6)
    }
}

static void render_vicii_specific_content(void* chip) {
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii) return;
    
    // This replaces the right column content from the original function
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

void vicii_gui_render_debug_window(void* chip, bool* show_window, const char* window_title) {
    if (!*show_window) return;
    
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii) {
        *show_window = false;
        return;
    }
    
    // Create generic chip GUI config
    chip_gui_config_t config = generic_chip_gui_get_default_config("MOS6567/69", "VIC-II");
    config.get_layout = get_vicii_layout;
    config.get_pin_states = get_vicii_pin_states;
    
    // Create generic chip GUI instance
    generic_chip_gui_t* gui = generic_chip_gui_create(vicii, &config);
    if (!gui) {
        return;
    }
    
    // Use generic chip GUI render function
    generic_chip_gui_render_debug_panel(gui, NULL, window_title, show_window, render_vicii_specific_content);
    
    // Cleanup
    generic_chip_gui_destroy(gui);
}

void vicii_gui_render_settings_window(void* chip, bool* show_window, const char* window_title) {
    if (!*show_window) return;
    
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii) {
        *show_window = false;
        return;
    }
    
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
}
