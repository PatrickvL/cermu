#include "chip/video/vic_ii/vicii_common.hpp"
#include "core/chip_layout.hpp"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif
#include <cstdio>
#include <cstring>

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase interface implementation
// ============================================================================

ChipLayout* vicii_base_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        // MOS6567/6569 VIC-II — 40-pin DIP
        ChipLayout layout = create_dip40_layout();

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
    }();
    return &layout;
}

// ============================================================================
// COMMON VIC-II GUI RENDERING FUNCTIONS
// ============================================================================

static const char* get_vicii_type_name(const vicii_base_t* vicii) {
    if (vicii && vicii->cached_chip_name) {
        return vicii->cached_chip_name;
    }
    return "Unknown VIC-II";
}

static const char* get_video_standard(const vicii_base_t* vicii) {
    if (vicii->cached_cycles_per_line == 65 && vicii->cached_total_lines == 262) {
        return "NTSC 60Hz";
    } else if (vicii->cached_cycles_per_line == 63 && vicii->cached_total_lines == 312) {
        return "PAL 50Hz";
    }
    return "Unknown";
}

// ============================================================================
// VIC-II PIN STATES
// ============================================================================

std::vector<PinSignalState> vicii_base_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, bus_snapshot_, [this](auto& ps) {
        // IRQ driven by VIC-II (override direction from generic)
        if (regs_[0x19] & 0x80) {
            ps[5].signal_level = false; // IRQ (pin 6) - active low, asserted
            ps[5].drive_direction = true;
            ps[5].high_impedance = false;
        }

        // Video output pins (always driven by VIC-II)
        ps[16].signal_level = true; // LUMA (pin 17)
        ps[16].drive_direction = true;
        ps[16].high_impedance = false;
        ps[17].signal_level = true; // CHROMA (pin 18)
        ps[17].drive_direction = true;
        ps[17].high_impedance = false;
        ps[18].signal_level = true; // CSYNC (pin 19)
        ps[18].drive_direction = true;
        ps[18].high_impedance = false;
    });
}

const char* vicii_base_t::get_layout_chip_name() const {
    return get_vicii_type_name(this);
}

// ============================================================================
// VIC-II GUI SETTINGS
// ============================================================================

void vicii_base_t::render_settings_content() {
    vicii_base_t* vicii = this;

    ImGui::Text("VIC-II Configuration");
    ImGui::Separator();
    
    ImGui::Text("Chip Type: %s", get_vicii_type_name(vicii));
    ImGui::Text("Video Standard: %s", get_video_standard(vicii));
    ImGui::Text("Timing: %d cycles/line, %d lines/frame", vicii->cached_cycles_per_line, vicii->cached_total_lines);
    
    ImGui::Separator();
    
    ImGui::Text("Display Settings");
    // Add interactive controls here later if needed
    ImGui::Text("(Settings controls will be added here)");
}

#endif // CERMU_HAS_GUI
