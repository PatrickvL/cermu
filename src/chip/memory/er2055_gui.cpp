// er2055_gui.cpp — ER2055 EAROM chip layout and pin visualization

#include "chip/memory/er2055.hpp"
#include "core/chip_layout.hpp"
#include "core/pin_macros.hpp"

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipLayout — ER2055 DIP-22 pinout
// ============================================================================
//
// Hardware-accurate pin assignment from GI ER2055 datasheet:

ChipLayout* ER2055::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip22_layout();

        layout.markings = {
            "512-bit EAROM",
            {}
        };

        PIN_LR(layout,  1, A0,   VCC,  22)   // address 0  / +5V
        PIN_LR(layout,  2, A1,   VSS,  21)   // address 1  / ground
        PIN_LR(layout,  3, A2,   A5,   20)   // address 2  / address 5
        PIN_LR(layout,  4, A3,   A4,   19)   // address 3  / address 4
        PIN_LR(layout,  5, D0,   D7,   18)   // data 0     / data 7
        PIN_LR(layout,  6, D1,   D6,   17)   // data 1     / data 6
        PIN_LR(layout,  7, D2,   D5,   16)   // data 2     / data 5
        PIN_LR(layout,  8, D3,   D4,   15)   // data 3     / data 4
        PIN_LR(layout,  9, C1,   CK,   14)   // control 1  / clock
        PIN_LR(layout, 10, C2,   _CS1, 13)   // control 2  / chip select 1 (active low)
        PIN_LR(layout, 11, GND,  CS2,  12)   // ground     / chip select 2 (active high)

        return layout;
    }();
    return &layout;
}

// ============================================================================
// Pin signal states — real-time visualization
// ============================================================================

std::vector<PinSignalState> ER2055::get_layout_pin_states(ChipLayout& layout) {
    auto pin_states = populate_pin_states_from_bus(layout, bus_snapshot_);
    return pin_states;
}

#endif // CERMU_HAS_GUI
