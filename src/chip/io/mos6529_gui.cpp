/*
 * mos6529_gui.cpp — MOS 6529B I/O Port Debug/Layout GUI
 *
 * 20-pin DIP pinout based on MOS 6529B datasheet.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/io/mos6529.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

#ifdef CERMU_HAS_GUI

// ============================================================================
// MOS 6529B — 20-pin DIP pinout (from MOS 6529B datasheet)
// ============================================================================

ChipLayout* mos6529_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip20_layout();

        //                 LEFT        RIGHT
        PIN_LR(layout,  1, RW,         VDD,          20);
        PIN_LR(layout,  2, P0,         _CS,          19);
        PIN_LR(layout,  3, P1,         D0,           18);
        PIN_LR(layout,  4, P2,         D1,           17);
        PIN_LR(layout,  5, P3,         D2,           16);
        PIN_LR(layout,  6, P4,         D3,           15);
        PIN_LR(layout,  7, P5,         D4,           14);
        PIN_LR(layout,  8, P6,         D5,           13);
        PIN_LR(layout,  9, P7,         D6,           12);
        PIN_LR(layout, 10, VSS,        D7,           11);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> mos6529_t::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, 0);

    // Mark port pins P0-P7 (left side pins 2-9, indices 1-8)
    for (int i = 0; i < 8; ++i) {
        ps[1 + i].signal_level    = (output_latch >> i) & 1;
        ps[1 + i].drive_direction = true;
        ps[1 + i].high_impedance  = false;
        ps[1 + i].signal_valid    = true;
    }

    return ps;
}

#endif // CERMU_HAS_GUI
