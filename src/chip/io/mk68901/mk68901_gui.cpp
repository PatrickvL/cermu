/*
 * mk68901_gui.cpp — MK68901 MFP chip layout
 *
 * Mostek/SGS-Thomson MK68901 Multi-Function Peripheral — 48-pin DIP.
 * Pinout from MK68901 datasheet.
 */
#include "chip/io/mk68901/mk68901.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* mk68901_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip48_layout();
        layout.markings.custom_text = "Multi-Function Peripheral";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, D0,         NC,          48);
        PIN_LR(layout,  2, D1,         VCC,         47);
        PIN_LR(layout,  3, D2,         I0,          46);
        PIN_LR(layout,  4, D3,         I1,          45);
        PIN_LR(layout,  5, D4,         I2,          44);
        PIN_LR(layout,  6, D5,         I3,          43);
        PIN_LR(layout,  7, D6,         I4,          42);
        PIN_LR(layout,  8, D7,         I5,          41);
        PIN_LR(layout,  9, GND,        I6,          40);
        PIN_LR(layout, 10, _CS,        I7,          39);
        PIN_LR(layout, 11, _DS,        _TR,         38);
        PIN_LR(layout, 12, RW,         _RR,         37);
        PIN_LR(layout, 13, _DTACK,     SO_USART,    36);
        PIN_LR(layout, 14, A1,         SI,          35);
        PIN_LR(layout, 15, A2,         TX_CLK,      34);
        PIN_LR(layout, 16, A3,         RC_USART,    33);
        PIN_LR(layout, 17, A4,         XTAL2,       32);
        PIN_LR(layout, 18, A5,         XTAL1,       31);
        PIN_LR(layout, 19, _IRQ,       TDO,         30);
        PIN_LR(layout, 20, IEI,        TCO,         29);
        PIN_LR(layout, 21, IEO,        TBO,         28);
        PIN_LR(layout, 22, _IACK,      TAO,         27);
        PIN_LR(layout, 23, CLK,        TBI,         26);
        PIN_LR(layout, 24, _RESET,     TAI,         25);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> mk68901_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
