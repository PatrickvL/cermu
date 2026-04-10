/*
 * gtia_gui.cpp — Atari GTIA chip layout
 *
 * CO14805 / CO14889 Graphic Television Interface Adapter — 40-pin DIP.
 * Pinout from Atari GTIA technical reference.
 */
#include "chip/video/gtia/gtia.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* gtia_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();
        layout.markings.custom_text = "Graphic Television Interface Adapter";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, CS0,        VCC,         40);
        PIN_LR(layout,  2, CS1,        NC,          39);
        PIN_LR(layout,  3, _CS3,       NC,          38);
        PIN_LR(layout,  4, PHI2,       NC,          37);
        PIN_LR(layout,  5, HALT,       _LPEN,       36);
        PIN_LR(layout,  6, A4,         AN2,         35);
        PIN_LR(layout,  7, A3,         AN0,         34);
        PIN_LR(layout,  8, A2,         AN1,         33);
        PIN_LR(layout,  9, A1,         RW,          32);
        PIN_LR(layout, 10, A0,         D7,          31);
        PIN_LR(layout, 11, D3,         D6,          30);
        PIN_LR(layout, 12, D2,         D5,          29);
        PIN_LR(layout, 13, D1,         D4,          28);
        PIN_LR(layout, 14, D0,         _FPHI0,      27);
        PIN_LR(layout, 15, TRIG3,      LUMA,        26);
        PIN_LR(layout, 16, TRIG2,      CSYNC,       25);
        PIN_LR(layout, 17, TRIG1,      COLOR,       24);
        PIN_LR(layout, 18, TRIG0,      P3,          23);
        PIN_LR(layout, 19, P0,         P2,          22);
        PIN_LR(layout, 20, GND,        P1,          21);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> gtia_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
