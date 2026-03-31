// ls139.cpp — vtable anchor + GUI stubs for LS139
//
// Every concrete ChipBase subclass needs out-of-line virtual definitions
// for create_chip_layout() and get_layout_pin_states() (declared under
// CERMU_HAS_GUI in chip.hpp) to anchor the vtable in a specific TU.

#include "chip/logic/ls139.hpp"

#ifdef CERMU_HAS_GUI
#include "core/chip_layout.hpp"

ChipLayout* LS139::create_chip_layout() const { return nullptr; }

std::vector<PinSignalState> LS139::get_layout_pin_states(ChipLayout& /*layout*/) {
    return {};
}
#endif

LS139::~LS139() = default;
