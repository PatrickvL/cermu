/*
 * huc6260_gui.cpp — HuC6260 VCE chip layout
 *
 * Hudson Soft HuC6260 Video Color Encoder — custom package.
 * This layout uses DIP-24 as an approximation showing the key
 * bus, palette, and video output signals.
 */
#include "chip/video/huc6260/huc6260.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* huc6260_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip24_layout();
        layout.markings.custom_text = "Video Color Encoder";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, GND,        VCC,         24);
        PIN_LR(layout,  2, D0,         CLK,         23);
        PIN_LR(layout,  3, D1,         _RD,         22);
        PIN_LR(layout,  4, D2,         _WR,         21);
        PIN_LR(layout,  5, D3,         _CS,         20);
        PIN_LR(layout,  6, D4,         A0,          19);
        PIN_LR(layout,  7, D5,         A1,          18);
        PIN_LR(layout,  8, D6,         A2,          17);
        PIN_LR(layout,  9, D7,         R_VID,       16);
        PIN_LR(layout, 10, HSYNC,      G_VID,       15);
        PIN_LR(layout, 11, VSYNC,      B_VID,       14);
        PIN_LR(layout, 12, CSYNC,      _RES,        13);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> huc6260_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
