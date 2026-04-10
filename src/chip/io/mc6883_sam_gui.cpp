/*
 * mc6883_sam_gui.cpp — MC6883 SAM chip layout
 *
 * Motorola MC6883 Synchronous Address Multiplexer — 40-pin DIP.
 * Pinout from MC6883 datasheet.
 */
#include "chip/io/mc6883_sam.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* mc6883_sam_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();
        layout.markings.custom_text = "Synchronous Address Multiplexer";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, A0,         VSS,         40);
        PIN_LR(layout,  2, A1,         _WE,         39);
        PIN_LR(layout,  3, A2,         RW,          38);
        PIN_LR(layout,  4, A3,         Q_CLK,       37);
        PIN_LR(layout,  5, A4,         ENABLE,      36);
        PIN_LR(layout,  6, A5,         S2,          35);
        PIN_LR(layout,  7, A6,         S1,          34);
        PIN_LR(layout,  8, A7,         S0,          33);
        PIN_LR(layout,  9, A8,         NC,          32);
        PIN_LR(layout, 10, A9,         VCC,         31);
        PIN_LR(layout, 11, A10,        _HSYNC,      30);
        PIN_LR(layout, 12, A11,        MA7,         29);
        PIN_LR(layout, 13, A12,        MA6,         28);
        PIN_LR(layout, 14, A13,        MA5,         27);
        PIN_LR(layout, 15, A14,        MA4,         26);
        PIN_LR(layout, 16, A15,        MA3,         25);
        PIN_LR(layout, 17, VSS,        MA2,         24);
        PIN_LR(layout, 18, D0,         MA1,         23);
        PIN_LR(layout, 19, D1,         MA0,         22);
        PIN_LR(layout, 20, _RAS0,      _RAS,        21);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> mc6883_sam_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
