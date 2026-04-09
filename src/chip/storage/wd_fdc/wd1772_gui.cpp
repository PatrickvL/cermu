/*
 * wd1772_gui.cpp — WD1772 FDC layout stub
 */
#include "chip/storage/wd_fdc/wd1772.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* wd1772_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> wd1772_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
