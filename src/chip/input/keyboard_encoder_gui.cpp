// keyboard_encoder_gui.cpp — Keyboard Encoder chip layout
//
// Generic keyboard encoder in a DIP-20 package, modeled after the
// SAB3021 / U807D serial keyboard encoder used in KC85-series computers.
//
// Pinout from SAB3021 datasheet (Siemens).
//
// Compiled only when CERMU_HAS_GUI is defined.

#include "chip/input/keyboard_encoder.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* KeyboardEncoder::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip20_layout();

        //           Left                          Right
        PIN_LR(layout,  1, COL0,     VCC,      20);
        PIN_LR(layout,  2, COL1,     ROW7,     19);
        PIN_LR(layout,  3, COL2,     ROW6,     18);
        PIN_LR(layout,  4, COL3,     ROW5,     17);
        PIN_LR(layout,  5, COL4,     ROW4,     16);
        PIN_LR(layout,  6, COL5,     ROW3,     15);
        PIN_LR(layout,  7, COL6,     ROW2,     14);
        PIN_LR(layout,  8, COL7,     ROW1,     13);
        PIN_LR(layout,  9, DATA,     ROW0,     12);
        PIN_LR(layout, 10, GND,      CLK,      11);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> KeyboardEncoder::get_layout_pin_states(ChipLayout& /*layout*/) {
    return {};
}

#endif // CERMU_HAS_GUI
