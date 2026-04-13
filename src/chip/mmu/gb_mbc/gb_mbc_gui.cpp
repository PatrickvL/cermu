// gb_mbc_gui.cpp — Game Boy MBC chip layout (stub)
//
// Compiled only with CERMU_HAS_GUI.

#ifdef CERMU_HAS_GUI

#include "chip/mmu/gb_mbc/gb_mbc.hpp"
#include "core/chip_layout.hpp"

// ── MBC chips are internal to the cartridge — no package layout ─────

template<const GbMbcTraits& T>
ChipLayout* gb_mbc_t<T>::create_chip_layout() const { return nullptr; }

template<const GbMbcTraits& T>
std::vector<PinSignalState> gb_mbc_t<T>::get_layout_pin_states(ChipLayout&) {
    return {};
}

// Explicit instantiations
template ChipLayout* gb_mbc_t<kMbcNoneTraits>::create_chip_layout() const;
template std::vector<PinSignalState> gb_mbc_t<kMbcNoneTraits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* gb_mbc_t<kMbc1Traits>::create_chip_layout() const;
template std::vector<PinSignalState> gb_mbc_t<kMbc1Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* gb_mbc_t<kMbc2Traits>::create_chip_layout() const;
template std::vector<PinSignalState> gb_mbc_t<kMbc2Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* gb_mbc_t<kMbc3Traits>::create_chip_layout() const;
template std::vector<PinSignalState> gb_mbc_t<kMbc3Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* gb_mbc_t<kMbc5Traits>::create_chip_layout() const;
template std::vector<PinSignalState> gb_mbc_t<kMbc5Traits>::get_layout_pin_states(ChipLayout&);

#endif // CERMU_HAS_GUI
