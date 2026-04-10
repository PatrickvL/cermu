/*
 * st_shifter_gui.cpp — Atari ST Shifter chip layout
 *
 * C025912 / C025913 Atari ST Shifter — custom 40-pin DIP.
 * Serialises 16-bit bitplane data into pixel output.
 * Pinout is approximate (Atari-proprietary).
 */
#include "chip/video/st_shifter/st_shifter.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* st_shifter_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();
        layout.markings.custom_text = "ST Shifter Video";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, VCC,        GND,         40);
        PIN_LR(layout,  2, CLK,        _CS,         39);
        PIN_LR(layout,  3, _RD,        _WR,         38);
        PIN_LR(layout,  4, RW,         _RES,        37);
        PIN_LR(layout,  5, D0,         A0,          36);
        PIN_LR(layout,  6, D1,         A1,          35);
        PIN_LR(layout,  7, D2,         A2,          34);
        PIN_LR(layout,  8, D3,         A3,          33);
        PIN_LR(layout,  9, D4,         A4,          32);
        PIN_LR(layout, 10, D5,         A5,          31);
        PIN_LR(layout, 11, D6,         DE,          30);
        PIN_LR(layout, 12, D7,         NC,          29);
        PIN_LR(layout, 13, D8,         R_VID,       28);
        PIN_LR(layout, 14, D9,         G_VID,       27);
        PIN_LR(layout, 15, D10,        B_VID,       26);
        PIN_LR(layout, 16, D11,        NC,          25);
        PIN_LR(layout, 17, D12,        HSYNC,       24);
        PIN_LR(layout, 18, D13,        VSYNC,       23);
        PIN_LR(layout, 19, D14,        NC,          22);
        PIN_LR(layout, 20, D15,        NC,          21);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> st_shifter_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
