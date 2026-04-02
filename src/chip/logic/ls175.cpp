// ls175.cpp — vtable anchor + GUI stubs for LS175
//
// Every concrete ChipBase subclass needs out-of-line virtual definitions
// for create_chip_layout() and get_layout_pin_states() (declared under
// CERMU_HAS_GUI in chip.hpp) to anchor the vtable in a specific TU.

#include "chip/logic/ls175.hpp"

#ifdef CERMU_HAS_GUI
#include "core/chip_layout.hpp"

// 74LS175 — Quad D flip-flop with clear — DIP-16
// Pinout from TI SN74LS175 datasheet
ChipLayout* LS175::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip16_layout();

        PIN_LR(layout,  1, _CLR,    VCC,     16);
        PIN_LR(layout,  2, FF1_Q,   FF4_Q,   15);
        PIN_LR(layout,  3, _FF1_Q,  _FF4_Q,  14);
        PIN_LR(layout,  4, FF1_D,   FF4_D,   13);
        PIN_LR(layout,  5, FF2_D,   FF3_D,   12);
        PIN_LR(layout,  6, _FF2_Q,  _FF3_Q,  11);
        PIN_LR(layout,  7, FF2_Q,   FF3_Q,   10);
        PIN_LR(layout,  8, GND,     CLK,      9);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> LS175::get_layout_pin_states(ChipLayout& /*layout*/) {
    return {};
}
#endif

LS175::~LS175() = default;
