/*
 * mk68901_gui.cpp — MK68901 MFP layout stub
 */
#include "chip/io/mk68901/mk68901.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* mk68901_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> mk68901_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
