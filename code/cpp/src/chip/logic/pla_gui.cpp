// This file now contains the gui_render_pla_debug function
// moved from cimgui_interface.c for better organization

#include "pla.h"
#include "../../gui/cimgui_interface.h"
#include "../../gui/generic_chip_gui.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#include "../../systems/c64/c64_bus.h"
#include "../../systems/c64/c64.h"
#include "../../chip/video/vic_ii/vicii_common.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>
#include <string.h>

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
    
    // Left side pins (1-14)
    PIN_LR(layout, 1, A15,     28, VCC);     // Address 15     / +5V Power
    PIN_LR(layout, 2, A14,     27, A8);      // Address 14     / Address 8
    PIN_LR(layout, 3, A13,     26, PHI2);    // Address 13     / Clock
    PIN_LR(layout, 4, A12,     25, RW);      // Address 12     / Read/Write
    PIN_LR(layout, 5, BA,      24, EXROM);   // Bus Available  / External ROM
    PIN_LR(layout, 6, AEC,     23, GAME);    // Address Enable / Game Line
    PIN_LR(layout, 7, P0,      22, ROMH);    // 6510 Port 0    / ROM High
    PIN_LR(layout, 8, P1,      21, ROML);    // 6510 Port 1    / ROM Low
    PIN_LR(layout, 9, P2,      20,  IO);     // 6510 Port 2    / I/O Select     
    PIN_LR(layout, 10, CHAREN, 19, GRW);     // Character Enable / Graphics R/W
    PIN_LR(layout, 11, HIRAM,  18, CHAROM);  // High RAM       / Character ROM
    PIN_LR(layout, 12, LORAM,  17, KERNAL);  // Low RAM        / KERNAL ROM
    PIN_LR(layout, 13, CAS,    16, BASIC);   // Column Addr Strobe / BASIC ROM
    PIN_LR(layout, 14, VSS,    15, CASRAM_PLA);  // Ground     / CAS RAM      
    
    return layout;
}

// Callback functions for generic chip GUI
static ChipLayout get_pla_layout(void* chip) {
    return create_pla_layout();
}

static void get_pla_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, struct PinSignalState* pin_states) {
    c64_t* c64 = (c64_t*)chip;
    if (!c64 || !layout || !pin_states) return;
    
    int total_pins = 28; // PLA is 28-pin DIP
    
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
    pin_states[13].signal_level = false; // VSS (Ground, pin 14)
    pin_states[27].signal_level = true;  // VCC (+5V, pin 28)
    
    // Set address pins based on current bus state (simplified)
    uint8_t current_mode = c64->bus.pla_banking_mode;
    pin_states[9].signal_level = (current_mode & 0x04) != 0;   // CHAREN (pin 10)
    pin_states[10].signal_level = (current_mode & 0x02) != 0;  // HIRAM (pin 11)
    pin_states[11].signal_level = (current_mode & 0x01) != 0;  // LORAM (pin 12)
    pin_states[22].signal_level = (current_mode & 0x10) != 0;  // GAME (pin 23)
    pin_states[23].signal_level = (current_mode & 0x08) != 0;  // EXROM (pin 24)
}

static void render_pla_specific_content(void* chip) {
    c64_t* c64 = (c64_t*)chip;
    if (!c64) return;
    
    // This replaces the right column content from the original function
    // Mode tracking and control
    bool has_c64 = c64;
    uint8_t current_mode = has_c64 ? c64->bus.pla_banking_mode : 0;
    
    // Static state for PLA debug window
    static bool auto_track_mode = true;
    static int pla_debug_selected_mode = 0;
    
    // Auto-track mode checkbox
    igCheckbox("Auto-track active mode", &auto_track_mode);
    
    if (auto_track_mode && has_c64) {
        pla_debug_selected_mode = current_mode;
    }
    
    igSameLine(0, -1.0f);
    igText("Current Mode: %d", current_mode);
    
    // Manual mode selector as active-low toggles in requested order: #LORAM, #HIRAM, #GAME, #EXROM, #CHAREN
    static bool loram_n = false, hiram_n = false, game_n = false, exrom_n = false, charen_n = false;
    // Extract bits from current mode (active-low)
    loram_n = ((pla_debug_selected_mode & 0x01) == 0);
    hiram_n = ((pla_debug_selected_mode & 0x02) == 0);
    game_n  = ((pla_debug_selected_mode & 0x10) == 0);
    exrom_n = ((pla_debug_selected_mode & 0x08) == 0);
    charen_n = ((pla_debug_selected_mode & 0x04) == 0);

    bool changed = false;
    igText("Viewing Mode:");
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#LORAM", &loram_n);
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#HIRAM", &hiram_n);
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#GAME", &game_n);
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#EXROM", &exrom_n);
    igSameLine(0, -1.0f);
    changed |= igCheckbox("#CHAREN", &charen_n);

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
    
    igSeparator();
    
    // Tab bar for CPU and VIC-II views
    if (igBeginTabBar("PLA Views", ImGuiTabBarFlags_None)) {
        
        // CPU Memory View Tab
        if (igBeginTabItem("CPU Memory View", NULL, ImGuiTabItemFlags_None)) {
            // CPU Memory Banking Table
            igText("CPU Memory Banking (16 x 4KB banks):");
            igText("Mode %d - %s", pla_debug_selected_mode,
                   (pla_debug_selected_mode == current_mode) ? "(ACTIVE)" : "(Preview)");
            igText("Configuration: %s", get_pla_mode_cpu_description(pla_debug_selected_mode));
            igEndTabItem();
        }
        
        igEndTabBar();
    }
}

// ============================================================================
// PLA GUI DEBUG WINDOW
// ============================================================================

void pla_render_debug_window(void* chip, bool* show_window) {
    // The chip parameter is expected to be a c64_t* since PLA is part of the C64 bus
    c64_t* c64 = (c64_t*)chip;
    
    if (!c64 || !*show_window) {
        if (show_window) *show_window = false;
        return;
    }
    
    // Create generic chip GUI config
    chip_gui_config_t config = generic_chip_gui_get_default_config("906114-01", "PLA");
    config.get_layout = get_pla_layout;
    config.get_pin_states = get_pla_pin_states;
    
    // Create generic chip GUI instance
    generic_chip_gui_t* gui = generic_chip_gui_create(c64, &config);
    if (!gui) {
        return;
    }
    
    // Use generic chip GUI render function
    generic_chip_gui_render_debug_panel(gui, NULL, "PLA Debug", show_window, render_pla_specific_content);
    
    // Cleanup
    generic_chip_gui_destroy(gui);
}
