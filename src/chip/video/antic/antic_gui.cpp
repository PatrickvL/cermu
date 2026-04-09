/*
 * antic_gui.cpp — Atari ANTIC layout stub
 */
#include "chip/video/antic/antic.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* antic_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> antic_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
