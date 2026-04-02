// bbc_vidproc_gui.cpp — BBC Micro Video ULA chip layout
//
// The Video ULA (VIDPROC) is a Ferranti custom gate array used in the
// BBC Micro.  The actual package is a 68-pin custom carrier; PLCC-68
// is used here as the closest standard representation.
//
// Pin assignments are derived from BBC Micro Model B schematics.
// Many internal signals are undocumented — only the externally visible
// signals from the schematic are labeled.
//
// Compiled only when CERMU_HAS_GUI is defined.

#include "chip/video/bbc_vidproc/bbc_vidproc.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* bbc_vidproc_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_plcc68_layout();

        // PLCC-68: 17 pins per side (left 1-17, bottom 18-34, right 35-51, top 52-68)
        // Pin assignments based on BBC Micro schematic signal groups

        // Left pins 1-17 (top→bottom) / Right pins 51-35 (top→bottom)
        PIN_LR(layout,  1, VCC,      R_VID,    51);
        PIN_LR(layout,  2, D0,       G_VID,    50);
        PIN_LR(layout,  3, D1,       B_VID,    49);
        PIN_LR(layout,  4, D2,       NC,       48);
        PIN_LR(layout,  5, D3,       NC,       47);
        PIN_LR(layout,  6, D4,       NC,       46);
        PIN_LR(layout,  7, D5,       NC,       45);
        PIN_LR(layout,  8, D6,       NC,       44);
        PIN_LR(layout,  9, D7,       NC,       43);
        PIN_LR(layout, 10, A0,       NC,       42);
        PIN_LR(layout, 11, _CS,      NC,       41);
        PIN_LR(layout, 12, _WR,      NC,       40);
        PIN_LR(layout, 13, DISEN,    NC,       39);
        PIN_LR(layout, 14, CURSOR,   NC,       38);
        PIN_LR(layout, 15, INV,      NC,       37);
        PIN_LR(layout, 16, CLK_2M,   NC,       36);
        PIN_LR(layout, 17, CLK_1M,   GND,      35);

        // Top pins 68-52 (left→right) / Bottom pins 18-34 (left→right)
        PIN_TB(layout, 68, TTEXT,    NC,       18);
        PIN_TB(layout, 67, RA0,      NC,       19);
        PIN_TB(layout, 66, RA1,      NC,       20);
        PIN_TB(layout, 65, RA2,      NC,       21);
        PIN_TB(layout, 64, CRTC_CLK, NC,       22);
        PIN_TB(layout, 63, NC,       NC,       23);
        PIN_TB(layout, 62, NC,       NC,       24);
        PIN_TB(layout, 61, NC,       NC,       25);
        PIN_TB(layout, 60, NC,       NC,       26);
        PIN_TB(layout, 59, NC,       NC,       27);
        PIN_TB(layout, 58, NC,       NC,       28);
        PIN_TB(layout, 57, NC,       NC,       29);
        PIN_TB(layout, 56, NC,       NC,       30);
        PIN_TB(layout, 55, NC,       NC,       31);
        PIN_TB(layout, 54, NC,       NC,       32);
        PIN_TB(layout, 53, VCC,      NC,       33);
        PIN_TB(layout, 52, _RESET,   NC,       34);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> bbc_vidproc_t::get_layout_pin_states(ChipLayout& /*layout*/) {
    return {};
}

#endif // CERMU_HAS_GUI
