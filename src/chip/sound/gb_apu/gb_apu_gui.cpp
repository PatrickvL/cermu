/*
 * gb_apu_gui.cpp — Game Boy APU layout stub
 */
#include "chip/sound/gb_apu/gb_apu.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* gb_apu_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> gb_apu_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
