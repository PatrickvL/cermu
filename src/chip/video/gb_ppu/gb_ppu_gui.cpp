/*
 * gb_ppu_gui.cpp — Game Boy PPU layout stub
 */
#include "chip/video/gb_ppu/gb_ppu.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI

ChipLayout* gb_ppu_t::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> gb_ppu_t::get_layout_pin_states(ChipLayout& layout) {
    return {};
}

#endif
