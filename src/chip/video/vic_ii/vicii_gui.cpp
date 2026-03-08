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

#ifdef CERMU_HAS_GUI

// ============================================================================
// COMMON VIC-II GUI RENDERING FUNCTIONS
// ============================================================================

static const char* get_vicii_type_name(const vicii_t* vicii) {
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

#endif // CERMU_HAS_GUI (layout/pin helpers)

// Class method implementation
#ifdef CERMU_HAS_GUI
void vicii_t::render_settings_content() {
    vicii_t* vicii = this;

    ImGui::Text("VIC-II Configuration");
    ImGui::Separator();
    
    ImGui::Text("Chip Type: %s", get_vicii_type_name(vicii));
    ImGui::Text("Video Standard: %s", get_video_standard(vicii));
    ImGui::Text("Timing: %d cycles/line, %d lines/frame", vicii->config->cycles_per_line, vicii->config->total_lines);
    
    ImGui::Separator();
    
    ImGui::Text("Display Settings");
    // Add interactive controls here later if needed
    ImGui::Text("(Settings controls will be added here)");
}
#endif // CERMU_HAS_GUI

// ============================================================================
// VIC-II layout virtuals
// ============================================================================

#ifdef CERMU_HAS_GUI

ChipLayout* vicii_t::create_chip_layout() const {
    static ChipLayout layout = create_vicii_layout();
    return &layout;
}

std::vector<PinSignalState> vicii_t::get_layout_pin_states(ChipLayout& layout) {
    return get_vicii_pin_states(this, &layout, bus_snapshot_);
}

const char* vicii_t::get_layout_chip_name() const {
    return get_vicii_type_name(this);
}

#endif // CERMU_HAS_GUI
