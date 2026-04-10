/*
 * genesis_315_5313_gui.cpp — Sega Genesis VDP chip layout
 *
 * Yamaha YM7101 / Sega 315-5313 — custom QFP package.
 * The actual IC uses a proprietary high-pin-count QFP; this layout
 * uses QFP-64 as an approximation showing the key bus and video signals.
 */
#include "chip/video/tms9918/genesis_315_5313.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* genesis_vdp_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_qfp64_layout();
        layout.markings.custom_text = "Video Display Processor";

        // Left pins 1-16 / Right pins 48-33
        PIN_LR(layout,  1, VCC,        D0,          48);
        PIN_LR(layout,  2, _INT,       D1,          47);
        PIN_LR(layout,  3, _CS,        D2,          46);
        PIN_LR(layout,  4, _RD,        D3,          45);
        PIN_LR(layout,  5, _WR,        D4,          44);
        PIN_LR(layout,  6, A0,         D5,          43);
        PIN_LR(layout,  7, A1,         D6,          42);
        PIN_LR(layout,  8, A2,         D7,          41);
        PIN_LR(layout,  9, A3,         D8,          40);
        PIN_LR(layout, 10, A4,         D9,          39);
        PIN_LR(layout, 11, AD0,        D10,         38);
        PIN_LR(layout, 12, AD1,        D11,         37);
        PIN_LR(layout, 13, AD2,        D12,         36);
        PIN_LR(layout, 14, AD3,        D13,         35);
        PIN_LR(layout, 15, AD4,        D14,         34);
        PIN_LR(layout, 16, AD5,        D15,         33);

        // Top pins 64-49 / Bottom pins 17-32
        PIN_TB(layout, 64, CLK,        AD6,         17);
        PIN_TB(layout, 63, _RES,       AD7,         18);
        PIN_TB(layout, 62, VCC,        _RAS,        19);
        PIN_TB(layout, 61, _CAS,       _WE,         20);
        PIN_TB(layout, 60, _OE,        GND,         21);
        PIN_TB(layout, 59, CSYNC,      NC,          22);
        PIN_TB(layout, 58, HSYNC,      NC,          23);
        PIN_TB(layout, 57, VSYNC,      NC,          24);
        PIN_TB(layout, 56, R_VID,      NC,          25);
        PIN_TB(layout, 55, G_VID,      NC,          26);
        PIN_TB(layout, 54, B_VID,      NC,          27);
        PIN_TB(layout, 53, Y_VID,      NC,          28);
        PIN_TB(layout, 52, VCC,        NC,          29);
        PIN_TB(layout, 51, HL,         NC,          30);
        PIN_TB(layout, 50, NC,         NC,          31);
        PIN_TB(layout, 49, GND,        NC,          32);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> genesis_vdp_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
