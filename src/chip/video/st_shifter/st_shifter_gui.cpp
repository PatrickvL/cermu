/*
 * st_shifter_gui.cpp — Atari ST Shifter layout stub
 */
#include "chip/video/st_shifter/st_shifter.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* st_shifter_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> st_shifter_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
