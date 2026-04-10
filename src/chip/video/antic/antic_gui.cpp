/*
 * antic_gui.cpp — Atari ANTIC chip layout
 *
 * CO12296 Alpha-Numeric Television Interface Controller — 40-pin DIP.
 * Pinout from Atari ANTIC technical reference.
 */
#include "chip/video/antic/antic.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* antic_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();
        layout.markings.custom_text = "Alpha-Numeric Television Interface Controller";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, VSS,        VSS,         40);
        PIN_LR(layout,  2, D2,         D1,          39);
        PIN_LR(layout,  3, D3,         D0,          38);
        PIN_LR(layout,  4, D4,         FPHI0,       37);
        PIN_LR(layout,  5, D5,         A15,         36);
        PIN_LR(layout,  6, D6,         A14,         35);
        PIN_LR(layout,  7, D7,         A13,         34);
        PIN_LR(layout,  8, AN0,        A12,         33);
        PIN_LR(layout,  9, AN1,        A11,         32);
        PIN_LR(layout, 10, AN2,        A10,         31);
        PIN_LR(layout, 11, PHI0,       A9,          30);
        PIN_LR(layout, 12, PHI2,       A8,          29);
        PIN_LR(layout, 13, _LPEN,      A7,          28);
        PIN_LR(layout, 14, _HALT,      A6,          27);
        PIN_LR(layout, 15, _RES,       A5,          26);
        PIN_LR(layout, 16, _NMI,       A4,          25);
        PIN_LR(layout, 17, RDY,        A3,          24);
        PIN_LR(layout, 18, RW,         A2,          23);
        PIN_LR(layout, 19, _REF,       A1,          22);
        PIN_LR(layout, 20, VCC,        A0,          21);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> antic_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
