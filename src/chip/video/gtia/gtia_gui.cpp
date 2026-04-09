/*
 * gtia_gui.cpp — Atari GTIA layout stub
 */
#include "chip/video/gtia/gtia.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* gtia_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> gtia_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
