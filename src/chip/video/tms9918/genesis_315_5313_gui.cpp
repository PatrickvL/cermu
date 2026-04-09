/*
 * genesis_315_5313_gui.cpp — Sega Genesis VDP layout stub
 */
#include "chip/video/tms9918/genesis_315_5313.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* genesis_vdp_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> genesis_vdp_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
