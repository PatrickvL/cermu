/*
 * vic_gui.cpp — MOS 6560/6561 VIC Debug/Settings GUI Windows
 *
 * Hardware-accurate 40-pin DIP pinout based on the MOS 6560/6561 datasheets.
 * The VIC (Video Interface Chip) generates video display, produces 4-channel
 * audio (3 square wave + 1 noise), and handles light pen input.
 *
 * MOS 6560 = NTSC variant (VIC-20 NTSC)
 * MOS 6561 = PAL variant  (VIC-20 PAL)
 *
 * Pinout reference: MOS Technology MOS 6560/6561 Datasheet (1980)
 */

#include "vic_common.h"
#include "../../../core/chip_layout.h"
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#endif
#include <cstdio>

#ifdef CERMU_HAS_GUI

// ============================================================================
// MOS 6560/6561 VIC LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_vic_layout(const char* part_number) {
    ChipLayout layout = create_dip40_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        part_number,
        "MOS Technology",
        {}, {}, {}, {},
        true, true, false, false
    };

    // Hardware-accurate MOS 6560/6561 VIC pinout (40-pin DIP)
    PIN_LR(layout,  1, D7,      VDD, 40)       // data hi / +5V
    PIN_LR(layout,  2, D6,      CSYNC, 39)     // / comp sync
    PIN_LR(layout,  3, D5,      LUMA, 38)      // / luminance
    PIN_LR(layout,  4, D4,      CHROMA, 37)    // / chrominance
    PIN_LR(layout,  5, D3,      RW, 36)        // / R/W
    PIN_LR(layout,  6, D2,      A13, 35)       // / addr hi
    PIN_LR(layout,  7, D1,      A12, 34)
    PIN_LR(layout,  8, D0,      A11, 33)       // data lo
    PIN_LR(layout,  9, MA7,     A10, 32)       // mux bus hi / addr
    PIN_LR(layout, 10, MA6,     A9, 31)
    PIN_LR(layout, 11, MA5,     A8, 30)        // / addr lo
    PIN_LR(layout, 12, MA4,     A7, 29)
    PIN_LR(layout, 13, MA3,     A6, 28)
    PIN_LR(layout, 14, MA2,     A5, 27)
    PIN_LR(layout, 15, MA1,     SOUND, 26)     // mux bus lo / audio
    PIN_LR(layout, 16, MA0,     POTX, 25)      // / paddle X
    PIN_LR(layout, 17, PHI0,    POTY, 24)      // clock in / paddle Y
    PIN_LR(layout, 18, PHI1,    LIGHT_PEN, 23) // clock phi1 / light pen
    PIN_LR(layout, 19, PHI2,    _CS, 22)       // clock phi2 / chip sel
    PIN_LR(layout, 20, VSS,     _IRQ, 21)      // gnd / interrupt

    return layout;
}

// ============================================================================
// VIC PIN STATES
// ============================================================================

static std::vector<PinSignalState> get_vic_pin_states(vic_base_t* vic, const ChipLayout* layout, bus_state_t bus_state) {
    if (!vic || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto pin_states = populate_pin_states_from_bus(*layout, bus_state);

    // VIC specific: Video output pins (always driven)
    pin_states[37].signal_level = true; // LUMA (pin 38)
    pin_states[37].drive_direction = true;
    pin_states[37].high_impedance = false;
    pin_states[36].signal_level = true; // CHROMA (pin 37)
    pin_states[36].drive_direction = true;
    pin_states[36].high_impedance = false;
    pin_states[38].signal_level = true; // CSYNC (pin 39)
    pin_states[38].drive_direction = true;
    pin_states[38].high_impedance = false;

    // Sound output (pin 26) — active if any voice is enabled
    bool any_voice_on = false;
    for (int v = VIC_REG_BASS_FREQ; v <= VIC_REG_NOISE_FREQ; v++) {
        if (vic->registers[v] & VIC_VOICE_ENABLE) {
            any_voice_on = true;
            break;
        }
    }
    pin_states[25].signal_level = any_voice_on; // SOUND (pin 26)
    pin_states[25].drive_direction = true;
    pin_states[25].high_impedance = false;

    return pin_states;
}

static const char* get_vic_type_name(const vic_base_t* vic) {
    if (vic->is_pal) return "MOS 6561 (PAL)";
    return "MOS 6560 (NTSC)";
}

#endif // CERMU_HAS_GUI (layout/pin helpers)

// ============================================================================
// ChipBase interface implementation
// ============================================================================

#ifdef CERMU_HAS_GUI

bool vic_base_t::has_settings_content() const { return true; }

// ============================================================================
// VIC layout virtuals
// ============================================================================

ChipLayout* vic_base_t::create_chip_layout() const {
    static ChipLayout layout_ntsc = create_vic_layout("MOS6560");
    static ChipLayout layout_pal  = create_vic_layout("MOS6561");
    return is_pal ? &layout_pal : &layout_ntsc;
}

std::vector<PinSignalState> vic_base_t::get_layout_pin_states(ChipLayout& layout) {
    return get_vic_pin_states(this, &layout, bus_snapshot_);
}

const char* vic_base_t::get_layout_chip_name() const {
    return get_vic_type_name(this);
}

#endif // CERMU_HAS_GUI

// ============================================================================
// VIC GUI SETTINGS
// ============================================================================

#ifdef CERMU_HAS_GUI
void vic_base_t::render_settings_content() {
    vic_base_t* vic = this;

    ImGui::Text("Video Interface Chip - %s Configuration", get_vic_type_name(vic));
    ImGui::Separator();
    ImGui::Text("Chip Type: %s", get_vic_type_name(vic));
    ImGui::Text("Clock: %u Hz", vic->clock_frequency);
    ImGui::Text("Lines: %u lines/frame", vic->total_lines);
    ImGui::Text("Cycles/line: %u", vic->cycles_per_line);

    ImGui::Separator();

    // Raw register dump
    if (ImGui::CollapsingHeader("Raw Registers ($9000-$900F)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char* reg_names[] = {
            "Control 1", "Control 2", "Video Matrix", "Rows",
            "Raster", "Char Base", "Light Pen X", "Light Pen Y",
            "Paddle X", "Paddle Y", "Bass Freq", "Alto Freq",
            "Soprano Freq", "Noise Freq", "Aux Color", "Background"
        };
        for (int i = 0; i < 16; i++) {
            ImGui::Text("$%04X ($%02X) %-13s: $%02X", 0x9000 + i, i,
                        reg_names[i], vic->registers[i]);
        }
    }
}
#endif // CERMU_HAS_GUI