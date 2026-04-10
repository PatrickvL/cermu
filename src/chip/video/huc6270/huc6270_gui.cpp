/*
 * huc6270_gui.cpp — HuC6270 VDC chip layout
 *
 * Hudson Soft HuC6270 Video Display Controller — custom QFP package.
 * This layout uses QFP-64 as an approximation showing the key
 * CPU bus, VRAM bus, and control signals.
 */
#include "chip/video/huc6270/huc6270.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* huc6270_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_qfp64_layout();
        layout.markings.custom_text = "Video Display Controller";

        // Left pins 1-16 / Right pins 48-33
        PIN_LR(layout,  1, VCC,        D0,          48);
        PIN_LR(layout,  2, _INT,       D1,          47);
        PIN_LR(layout,  3, _CS,        D2,          46);
        PIN_LR(layout,  4, _RD,        D3,          45);
        PIN_LR(layout,  5, _WR,        D4,          44);
        PIN_LR(layout,  6, A0,         D5,          43);
        PIN_LR(layout,  7, A1,         D6,          42);
        PIN_LR(layout,  8, HSYNC,      D7,          41);
        PIN_LR(layout,  9, VSYNC,      MA0,         40);
        PIN_LR(layout, 10, DE,         MA1,         39);
        PIN_LR(layout, 11, GND,        MA2,         38);
        PIN_LR(layout, 12, AD0,        MA3,         37);
        PIN_LR(layout, 13, AD1,        MA4,         36);
        PIN_LR(layout, 14, AD2,        MA5,         35);
        PIN_LR(layout, 15, AD3,        MA6,         34);
        PIN_LR(layout, 16, AD4,        MA7,         33);

        // Top pins 64-49 / Bottom pins 17-32
        PIN_TB(layout, 64, CLK,        AD5,         17);
        PIN_TB(layout, 63, _RES,       AD6,         18);
        PIN_TB(layout, 62, VCC,        AD7,         19);
        PIN_TB(layout, 61, _RAS,       MA8,         20);
        PIN_TB(layout, 60, _CAS,       MA9,         21);
        PIN_TB(layout, 59, _WE,        MA10,        22);
        PIN_TB(layout, 58, _OE,        MA11,        23);
        PIN_TB(layout, 57, NC,         MA12,        24);
        PIN_TB(layout, 56, NC,         MA13,        25);
        PIN_TB(layout, 55, NC,         MA14,        26);
        PIN_TB(layout, 54, NC,         MA15,        27);
        PIN_TB(layout, 53, NC,         NC,          28);
        PIN_TB(layout, 52, NC,         NC,          29);
        PIN_TB(layout, 51, NC,         NC,          30);
        PIN_TB(layout, 50, NC,         NC,          31);
        PIN_TB(layout, 49, GND,        NC,          32);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> huc6270_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
