/*
 * huc6270_gui.cpp — HuC6270 VDC layout stub
 */
#include "chip/video/huc6270/huc6270.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* huc6270_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> huc6270_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
