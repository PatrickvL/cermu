/*
 * ls259_gui.cpp — 74LS259 Addressable Latch Debug/Layout GUI
 *
 * 16-pin DIP pinout based on the TI SN74LS259 datasheet (SDLS132A).
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/logic/ls259.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

ChipLayout* LS259::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip16_layout();

        layout.markings.custom_text  = "Addressable Latch";

        //                  LEFT                      RIGHT
        PIN_LR(layout,  1, A0,         VCC,         16);
        PIN_LR(layout,  2, A1,         _CLR,        15);
        PIN_LR(layout,  3, A2,         _G,          14);
        PIN_LR(layout,  4, Q0,         D0,          13);
        PIN_LR(layout,  5, Q1,         Q7,          12);
        PIN_LR(layout,  6, Q2,         Q6,          11);
        PIN_LR(layout,  7, Q3,         Q5,          10);
        PIN_LR(layout,  8, GND,        Q4,           9);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> LS259::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, 0);

    uint8_t q = q_all();

    // Q0 (pin 4, idx 3)
    ps[3].signal_level = (q >> 0) & 1; ps[3].drive_direction = true;
    ps[3].high_impedance = false;      ps[3].signal_valid = true;
    // Q1 (pin 5, idx 4)
    ps[4].signal_level = (q >> 1) & 1; ps[4].drive_direction = true;
    ps[4].high_impedance = false;      ps[4].signal_valid = true;
    // Q2 (pin 6, idx 5)
    ps[5].signal_level = (q >> 2) & 1; ps[5].drive_direction = true;
    ps[5].high_impedance = false;      ps[5].signal_valid = true;
    // Q3 (pin 7, idx 6)
    ps[6].signal_level = (q >> 3) & 1; ps[6].drive_direction = true;
    ps[6].high_impedance = false;      ps[6].signal_valid = true;
    // Q4 (pin 9, idx 8)
    ps[8].signal_level = (q >> 4) & 1; ps[8].drive_direction = true;
    ps[8].high_impedance = false;      ps[8].signal_valid = true;
    // Q5 (pin 10, idx 9)
    ps[9].signal_level = (q >> 5) & 1; ps[9].drive_direction = true;
    ps[9].high_impedance = false;      ps[9].signal_valid = true;
    // Q6 (pin 11, idx 10)
    ps[10].signal_level = (q >> 6) & 1; ps[10].drive_direction = true;
    ps[10].high_impedance = false;      ps[10].signal_valid = true;
    // Q7 (pin 12, idx 11)
    ps[11].signal_level = (q >> 7) & 1; ps[11].drive_direction = true;
    ps[11].high_impedance = false;      ps[11].signal_valid = true;

    return ps;
}

#endif // CERMU_HAS_GUI
