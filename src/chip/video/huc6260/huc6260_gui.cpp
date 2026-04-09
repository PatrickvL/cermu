/*
 * huc6260_gui.cpp — HuC6260 VCE layout stub
 */
#include "chip/video/huc6260/huc6260.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* huc6260_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> huc6260_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
