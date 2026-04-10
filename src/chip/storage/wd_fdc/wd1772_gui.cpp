/*
 * wd1772_gui.cpp — WD1772 FDC chip layout
 *
 * Western Digital WD1772 — 28-pin DIP floppy disk controller.
 * Pinout from WD1772 datasheet.
 */
#include "chip/storage/wd_fdc/wd1772.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* wd1772_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip28_layout();
        layout.markings.custom_text = "Floppy Disk Controller";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, _CS,        _TEST,       28);
        PIN_LR(layout,  2, A0,         _INTRQ,      27);
        PIN_LR(layout,  3, A1,         DRQ,         26);
        PIN_LR(layout,  4, ENP,        CLK,         25);
        PIN_LR(layout,  5, RW,         _MR,         24);
        PIN_LR(layout,  6, GND,        VCC,         23);
        PIN_LR(layout,  7, DIRC,       D7,          22);
        PIN_LR(layout,  8, STEP,       D6,          21);
        PIN_LR(layout,  9, _DDEN,      D5,          20);
        PIN_LR(layout, 10, WDATA,      D4,          19);
        PIN_LR(layout, 11, WGATE,      D3,          18);
        PIN_LR(layout, 12, _TR00,      D2,          17);
        PIN_LR(layout, 13, _IP,        D1,          16);
        PIN_LR(layout, 14, _WPRT,      D0,          15);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> wd1772_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
