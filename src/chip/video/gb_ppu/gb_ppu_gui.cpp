/*
 * gb_ppu_gui.cpp — Game Boy PPU chip layout
 *
 * The PPU is integrated into the Sharp LR35902 / DMG-CPU SoC.
 * This layout represents the PPU's logical interface signals
 * (VRAM bus, LCD output, interrupts) rather than a physical package.
 */
#include "chip/video/gb_ppu/gb_ppu.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* gb_ppu_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();
        layout.markings.custom_text = "Pixel Processing Unit (SoC)";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, VCC,        GND,         40);
        PIN_LR(layout,  2, CLK,        _INT,        39);
        PIN_LR(layout,  3, _CS,        _RD,         38);
        PIN_LR(layout,  4, _WR,        RW,          37);
        PIN_LR(layout,  5, D0,         A0,          36);
        PIN_LR(layout,  6, D1,         A1,          35);
        PIN_LR(layout,  7, D2,         A2,          34);
        PIN_LR(layout,  8, D3,         A3,          33);
        PIN_LR(layout,  9, D4,         A4,          32);
        PIN_LR(layout, 10, D5,         A5,          31);
        PIN_LR(layout, 11, D6,         A6,          30);
        PIN_LR(layout, 12, D7,         A7,          29);
        PIN_LR(layout, 13, MA0,        A8,          28);
        PIN_LR(layout, 14, MA1,        A9,          27);
        PIN_LR(layout, 15, MA2,        A10,         26);
        PIN_LR(layout, 16, MA3,        A11,         25);
        PIN_LR(layout, 17, MA4,        A12,         24);
        PIN_LR(layout, 18, MA5,        HSYNC,       23);
        PIN_LR(layout, 19, MA6,        VSYNC,       22);
        PIN_LR(layout, 20, MA7,        DE,          21);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> gb_ppu_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
