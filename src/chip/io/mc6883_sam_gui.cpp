/*
 * mc6883_sam_gui.cpp — MC6883 SAM layout stub
 */
#include "chip/io/mc6883_sam.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* mc6883_sam_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> mc6883_sam_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
