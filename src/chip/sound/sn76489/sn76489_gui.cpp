/*
 * sn76489_gui.cpp — SN76489 PSG Debug/Layout GUI
 *
 * 16-pin DIP pinout based on the TI SN76489 datasheet (SLPS190).
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/sound/sn76489/sn76489.hpp"
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

ChipLayout* sn76489_t::create_chip_layout() const {
    static ChipLayout layout = [this] {
        ChipLayout layout = create_dip16_layout();

        // SN76489 16-pin DIP pinout (TI datasheet)
        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, D5,         VCC,         16);
        PIN_LR(layout,  2, D6,         D4,          15);
        PIN_LR(layout,  3, D7,         CLK,         14);
        PIN_LR(layout,  4, RDY,        D3,          13);  // READY
        PIN_LR(layout,  5, _WE,        D2,          12);
        PIN_LR(layout,  6, _CE,        D1,          11);
        PIN_LR(layout,  7, AUDIO_OUT,  D0,          10);
        PIN_LR(layout,  8, VSS,        NC,           9);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> sn76489_t::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, 0);

    // Audio output (pin 7, idx 6) — always driven
    ps[6].signal_level    = true;
    ps[6].drive_direction = true;
    ps[6].high_impedance  = false;
    ps[6].signal_valid    = true;

    return ps;
}

#endif // CERMU_HAS_GUI
