#include "vicii_common.h"
#include "../../../gui/cimgui_interface.h"
#include "../../../gui/generic_chip_gui.h"
#include "../../../core/non_cpu_chip_layouts.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
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

// Callback functions for generic chip GUI
static ChipLayout get_vicii_layout(void* chip) {
    return create_vicii_layout();
}

static void get_vicii_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, struct PinState* pin_states) {
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii || !layout || !pin_states) return;
    
    // For now, implement basic pin state logic
    // This would be enhanced with actual VIC-II state reading
    int total_pins = 40; // VIC-II is 40-pin DIP
    
    // Initialize all pins as inactive by default
    for (int i = 0; i < total_pins; i++) {
        pin_states[i].pin_number = i + 1;
        pin_states[i].is_active = false;
        pin_states[i].is_output = false;
        pin_states[i].value = 0;
        pin_states[i].is_tristate = false;
        pin_states[i].has_pullup = false;
        pin_states[i].has_pulldown = false;
        pin_states[i].is_valid = true;
        pin_states[i].analog_voltage = 0.0f;
        pin_states[i].is_pwm = false;
        pin_states[i].pwm_duty_cycle = 0.0f;
    }
    
    // Set power pins as active
    pin_states[0].is_active = true;  // VDD (pin 1)
    pin_states[39].is_active = true; // VCC (pin 40)
    
    // Set IRQ pin state based on VIC-II registers
    if (vicii->registers.data && (vicii->registers.data[0x19] & 0x80)) {
        pin_states[5].is_active = true; // IRQ (pin 6)
    }
}

static void render_vicii_specific_content(void* chip) {
    vicii_t* vicii = (vicii_t*)chip;
    if (!vicii) return;
    
    // This replaces the right column content from the original function
    igText("%s", get_vicii_type_name(vicii));
    igSeparator();
    
    // Basic chip information
    if (igCollapsingHeader_TreeNodeFlags("Chip Information", ImGuiTreeNodeFlags_DefaultOpen)) {
        igText("Video Standard: %s", get_video_standard(vicii));
        igText("Cycles per Line: %d", vicii->timing.cycles_per_line);
        igText("Total Lines: %d", vicii->timing.total_lines);
        igText("Current Bank: %d", vicii->memory.bank);
    }
    
    // Raster information
    if (igCollapsingHeader_TreeNodeFlags("Raster Information", ImGuiTreeNodeFlags_DefaultOpen)) {
        igText("Raster Line: %d", vicii->timing.raster_counter);
        igText("Raster Cycle: %d", vicii->timing.x_cycle);
        igText("Badline Condition: %s", vicii->video_logic.is_bad_line ? "YES" : "NO");
        igText("X Coordinate: %d", vicii->timing.x_coordinate);
        
        // Progress bar for raster position
        float raster_progress = (float)vicii->timing.raster_counter / (float)vicii->timing.total_lines;
        igProgressBar(raster_progress, (ImVec2){-1, 0}, NULL);
        igText("Raster Progress: %.1f%%", raster_progress * 100.0f);
    }
    
    // Control registers
    if (igCollapsingHeader_TreeNodeFlags("Control Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
        uint8_t cr1 = vicii->registers.data[0x11];
        uint8_t cr2 = vicii->registers.data[0x16];
        uint8_t memory_setup = vicii->registers.data[0x18];
        
        igText("Control Register 1 ($D011): $%02X", cr1);
        igIndent(20.0f);
        igText("RST8: %d  ECM: %d  BMM: %d  DEN: %d  RSEL: %d",
               (cr1 >> 7) & 1, (cr1 >> 6) & 1, (cr1 >> 5) & 1,
               (cr1 >> 4) & 1, (cr1 >> 3) & 1);
        igText("YSCROLL: %d", cr1 & 0x07);
        igUnindent(20.0f);
        
        igText("Control Register 2 ($D016): $%02X", cr2);
        igIndent(20.0f);
        igText("RES: %d  MCM: %d  CSEL: %d  XSCROLL: %d",
               (cr2 >> 5) & 1, (cr2 >> 4) & 1, (cr2 >> 3) & 1, cr2 & 0x07);
        igUnindent(20.0f);
        
        igText("Memory Setup ($D018): $%02X", memory_setup);
        igIndent(20.0f);
        igText("VM: %d  CB: %d", (memory_setup >> 4) & 0x0F, (memory_setup >> 1) & 0x07);
        igText("Video Matrix Base: $%04X", ((memory_setup >> 4) & 0x0F) * 0x400);
        igText("Character Base: $%04X", ((memory_setup >> 1) & 0x07) * 0x800);
        igUnindent(20.0f);
        
        igText("Current Screen Mode: %s", get_screen_mode(cr1, cr2));
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
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("VIC-II Configuration");
    igSeparator();
    
    igText("Chip Type: %s", get_vicii_type_name(vicii));
    igText("Video Standard: %s", get_video_standard(vicii));
    igText("Timing: %d cycles/line, %d lines/frame", vicii->timing.cycles_per_line, vicii->timing.total_lines);
    
    igSeparator();
    
    igText("Display Settings");
    // Add interactive controls here later if needed
    igText("(Settings controls will be added here)");

    igEnd();
}
