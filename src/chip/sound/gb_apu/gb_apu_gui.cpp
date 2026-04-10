/*
 * gb_apu_gui.cpp — Game Boy APU chip layout
 *
 * The APU is integrated into the Sharp LR35902 / DMG-CPU SoC.
 * This layout represents the APU's logical interface signals
 * (register bus, audio output) rather than a physical package.
 */
#include "chip/sound/gb_apu/gb_apu.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* gb_apu_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip14_layout();
        layout.markings.custom_text = "Audio Processing Unit (SoC)";

        //                  LEFT                        RIGHT
        PIN_LR(layout,  1, VCC,        GND,         14);
        PIN_LR(layout,  2, CLK,        _CS,         13);
        PIN_LR(layout,  3, RW,         D0,          12);
        PIN_LR(layout,  4, D1,         D2,          11);
        PIN_LR(layout,  5, D3,         D4,          10);
        PIN_LR(layout,  6, D5,         D6,           9);
        PIN_LR(layout,  7, D7,         AUDIO_OUT,    8);

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> gb_apu_t::get_layout_pin_states(ChipLayout& layout) {
    return build_pin_states(layout, 0);
}

#endif
