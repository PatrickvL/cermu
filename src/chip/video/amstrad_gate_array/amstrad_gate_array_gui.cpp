// amstrad_gate_array_gui.cpp — Amstrad Gate Array chip layout
//
// The Amstrad CPC uses custom gate array ICs (40007, 40010, 40226) in a
// 40-pin DIP package.  The gate array integrates PAL/colour encoding,
// memory bank switching, and interrupt generation.
//
// Pinout is based on the Amstrad CPC464 service manual schematics.
//
// Compiled only when CERMU_HAS_GUI is defined.

#include "chip/video/amstrad_gate_array/amstrad_gate_array.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* amstrad_gate_array_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();

        //           Left                          Right
        PIN_LR(layout,  1, _MREQ,    VCC,      40);
        PIN_LR(layout,  2, _M1,      _HSYNC,   39);
        PIN_LR(layout,  3, PHI,      _VSYNC,   38);
        PIN_LR(layout,  4, _RD,      _DISPEN,  37);
        PIN_LR(layout,  5, _IORQ,    _CCLK,    36);
        PIN_LR(layout,  6, A15,      B_VID,    35);
        PIN_LR(layout,  7, A14,      R_VID,    34);
        PIN_LR(layout,  8, D7,       G_VID,    33);
        PIN_LR(layout,  9, D6,       _CSYNC,   32);
        PIN_LR(layout, 10, D5,       _INT,     31);
        PIN_LR(layout, 11, D4,       RDY,      30);
        PIN_LR(layout, 12, D3,       _CAS,     29);
        PIN_LR(layout, 13, D2,       CCLK,     28);
        PIN_LR(layout, 14, D1,       PHI_N,    27);
        PIN_LR(layout, 15, D0,       RAS,      26);
        PIN_LR(layout, 16, _RESET,   _EN244,   25);
        PIN_LR(layout, 17, _ROMEN,   _CASAD,   24);
        PIN_LR(layout, 18, RAMDIS,   CPU_A,    23);
        PIN_LR(layout, 19, RAMRD,    _ROM,     22);
        PIN_LR(layout, 20, GND,      _LPEN,    21);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> amstrad_gate_array_t::get_layout_pin_states(ChipLayout& /*layout*/) {
    return {};
}

#endif // CERMU_HAS_GUI
