// ls138.cpp — vtable anchor + GUI stubs for LS138
//
// Every concrete ChipBase subclass needs out-of-line virtual definitions
// for create_chip_layout() and get_layout_pin_states() (declared under
// CERMU_HAS_GUI in chip.hpp) to anchor the vtable in a specific TU.

#include "chip/logic/ls138.hpp"

#ifdef CERMU_HAS_GUI
#include "core/chip_layout.hpp"

// 74LS138 — 3-to-8 line decoder/demultiplexer — DIP-16
// Pinout from TI SN74LS138 datasheet
ChipLayout* LS138::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip16_layout();

        PIN_LR(layout,  1, A0,    VCC,   16);
        PIN_LR(layout,  2, A1,    _Y0,   15);
        PIN_LR(layout,  3, A2,    _Y1,   14);
        PIN_LR(layout,  4, _E1,   _Y2,   13);
        PIN_LR(layout,  5, _E2,   _Y3,   12);
        PIN_LR(layout,  6, E3,    _Y4,   11);
        PIN_LR(layout,  7, _Y7,   _Y5,   10);
        PIN_LR(layout,  8, GND,   _Y6,    9);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> LS138::get_layout_pin_states(ChipLayout& /*layout*/) {
    return {};
}
#endif

LS138::~LS138() = default;
