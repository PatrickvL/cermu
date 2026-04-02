// ls139.cpp — vtable anchor + GUI stubs for LS139
//
// Every concrete ChipBase subclass needs out-of-line virtual definitions
// for create_chip_layout() and get_layout_pin_states() (declared under
// CERMU_HAS_GUI in chip.hpp) to anchor the vtable in a specific TU.

#include "chip/logic/ls139.hpp"

#ifdef CERMU_HAS_GUI
#include "core/chip_layout.hpp"

// 74LS139 — Dual 2-to-4 line decoder/demultiplexer — DIP-16
// Pinout from TI SN74LS139 datasheet
ChipLayout* LS139::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip16_layout();

        PIN_LR(layout,  1, _DEC1_G,  VCC,       16);
        PIN_LR(layout,  2, DEC1_A,   _DEC2_G,   15);
        PIN_LR(layout,  3, DEC1_B,   DEC2_A,    14);
        PIN_LR(layout,  4, _DEC1_Y0, DEC2_B,    13);
        PIN_LR(layout,  5, _DEC1_Y1, _DEC2_Y0,  12);
        PIN_LR(layout,  6, _DEC1_Y2, _DEC2_Y1,  11);
        PIN_LR(layout,  7, _DEC1_Y3, _DEC2_Y2,  10);
        PIN_LR(layout,  8, GND,      _DEC2_Y3,   9);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> LS139::get_layout_pin_states(ChipLayout& /*layout*/) {
    return {};
}
#endif

LS139::~LS139() = default;
