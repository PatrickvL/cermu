/*
 * ted7360_gui.cpp — TED 7360/8360 Debug/Settings GUI Windows
 *
 * Hardware-accurate 48-pin DIP pinout based on the MOS 7360 TED datasheet.
 * The TED (Text Editing Device) is a combined video, sound, and I/O controller
 * used in the Commodore 16, 116, and Plus/4 computers.  It generates video
 * display, produces 2-channel sound, scans the keyboard matrix, manages
 * memory banking (ROM/RAM), and contains three 16-bit countdown timers.
 *
 * Pinout reference: MOS Technology 7360 TED Datasheet (1984)
 */

#include "ted7360.h"
#include "../../../core/chip_layout.h"
#include "../../../core/pin_macros.h"
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#endif
#include <cstdio>
#include <memory>

#ifdef CERMU_HAS_GUI

// ============================================================================
// TED 7360 LAYOUT (48-pin DIP)
// ============================================================================

inline ChipLayout create_ted7360_layout() {
    ChipLayout layout = create_custom_dip(48, "TED7360");

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        "MOS7360 TED",
        "MOS Technology",
        {}, {}, {}, {},
        true, true, false, false
    };

    // Hardware-accurate TED 7360 pinout (48-pin DIP, 24 pins per side)
    PIN_LR(layout,  1, VSS,     VDD, 48)       // gnd / +5V supply
    PIN_LR(layout,  2, D0,      _RAS, 47)      // data bus lo
    PIN_LR(layout,  3, D1,      _CAS, 46)      // / DRAM strobes
    PIN_LR(layout,  4, D2,      MUX, 45)       // / addr mux
    PIN_LR(layout,  5, D3,      BA, 44)        // / bus available
    PIN_LR(layout,  6, D4,      _IRQ, 43)      // / interrupt
    PIN_LR(layout,  7, D5,      PHI0, 42)      // / clock in
    PIN_LR(layout,  8, D6,      PHI2, 41)      // / clock out
    PIN_LR(layout,  9, D7,      RW, 40)        // data bus hi / R/W
    PIN_LR(layout, 10, A0,      A15, 39)       // addr bus lo
    PIN_LR(layout, 11, A1,      A14, 38)
    PIN_LR(layout, 12, A2,      A13, 37)
    PIN_LR(layout, 13, A3,      A12, 36)
    PIN_LR(layout, 14, A4,      A11, 35)
    PIN_LR(layout, 15, A5,      A10, 34)
    PIN_LR(layout, 16, A6,      A9, 33)
    PIN_LR(layout, 17, A7,      A8, 32)        // / addr bus hi
    PIN_LR(layout, 18, K0,      K7, 31)        // keyboard matrix
    PIN_LR(layout, 19, K1,      K6, 30)
    PIN_LR(layout, 20, K2,      K5, 29)
    PIN_LR(layout, 21, K3,      K4, 28)
    PIN_LR(layout, 22, LUMA,    SOUND, 27)     // luminance / audio
    PIN_LR(layout, 23, CHROMA,  _CS1, 26)      // chrominance / chip sel
    PIN_LR(layout, 24, VSS,     CS0, 25)       // gnd / chip sel (hi)

    return layout;
}

// ============================================================================
// TED PIN STATES
// ============================================================================

static std::vector<PinSignalState> get_ted_pin_states(ted7360_t* ted, const ChipLayout* layout, bus_state_t bus_state) {
    if (!ted || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto pin_states = populate_pin_states_from_bus(*layout, bus_state);

    // TED specific: IRQ (pin 43, index 42) — driven by TED
    pin_states[42].signal_level = !ted->irq_pending();
    pin_states[42].drive_direction = true;
    pin_states[42].high_impedance = false;

    // BA pin (pin 44, index 43) — high when CPU has bus
    pin_states[43].signal_level = !ted->bus.ba_low;
    pin_states[43].drive_direction = true;
    pin_states[43].high_impedance = false;

    // Video output pins (always driven)
    pin_states[21].signal_level = true; // LUMA (pin 22)
    pin_states[21].drive_direction = true;
    pin_states[21].high_impedance = false;
    pin_states[22].signal_level = true; // CHROMA (pin 23)
    pin_states[22].drive_direction = true;
    pin_states[22].high_impedance = false;

    // Sound output (pin 27, index 26) — active if any channel enabled
    bool sound_active = ted->sound.ch1_enabled || ted->sound.ch2_enabled || ted->sound.noise_enabled;
    pin_states[26].signal_level = sound_active;
    pin_states[26].drive_direction = true;
    pin_states[26].high_impedance = false;

    return pin_states;
}

static ChipLayout& get_ted_layout() {
    static ChipLayout layout = create_ted7360_layout();
    return layout;
}

#endif // CERMU_HAS_GUI (layout/pin helpers)

// ============================================================================
// ChipBase interface implementation
// ============================================================================

#ifdef CERMU_HAS_GUI

bool ted7360_t::has_settings_content() const { return true; }

ChipLayout* ted7360_t::get_chip_layout() const {
    return &get_ted_layout();
}

std::vector<PinSignalState> ted7360_t::get_layout_pin_states(ChipLayout& layout) {
    return get_ted_pin_states(this, &layout, bus_snapshot_);
}

#endif // CERMU_HAS_GUI

// ============================================================================
// TED 7360 GUI SETTINGS WINDOW
// ============================================================================

#ifdef CERMU_HAS_GUI
void ted7360_t::render_settings_content() {
    ted7360_t* ted = this;

    ImGui::Text("TED 7360 - Text Editing Device Configuration");
    ImGui::Separator();
    ImGui::Text("Chip Type: MOS 7360 TED (48-pin DIP)");
    ImGui::Text("Standard: %s", ted->timing.is_pal ? "PAL" : "NTSC");
    ImGui::Text("Lines/frame: %d", ted->timing.lines_per_frame);
    ImGui::Text("CPU cycles/line: %d", ted->timing.cpu_cycles_per_line);

    ImGui::Separator();

    // Raw register dump
    if (ImGui::CollapsingHeader("Raw Registers ($FF00-$FF1F)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char* reg_names[] = {
            "Timer1 Lo",  "Timer1 Hi",  "Timer2 Lo",  "Timer2 Hi",
            "Timer3 Lo",  "Timer3 Hi",  "Control 1",  "Control 2",
            "Keyboard",   "IRQ Status", "IRQ Mask",   "Cursor Lo",
            "Cursor Hi",  "Sound1 Lo",  "Sound1 Hi",  "Sound2 Lo",
            "Sound2 Hi",  "Sound Ctrl", "Mem Ctrl",   "Char Hi",
            "Bitmap Addr","Color BG0",  "Color BG1",  "Color BG2",
            "Color BG3",  "Border",     "CharPos Hi", "Raster Lo",
            "VPos",       "HPos",       "Flash",      "ROM/RAM"
        };
        for (int i = 0; i < TED_NUM_REGS; i++) {
            ImGui::Text("$FF%02X %-12s: $%02X", i, reg_names[i], ted->registers.data[i]);
        }
    }
}
#endif // CERMU_HAS_GUI