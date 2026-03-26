#include "core/chip.hpp"

#ifdef CERMU_HAS_GUI
#include "core/chip_layout.hpp"

ChipLayout* ChipBase::get_chip_layout() const {
    if (!layout_initialized_) {
        layout_ = create_chip_layout();
        if (layout_) layout_->chip_info = &info_;
        layout_initialized_ = true;
    }
    return layout_;
}

std::vector<PinSignalState> ChipBase::get_layout_pin_states(ChipLayout&) { return {}; }
const char* ChipBase::get_layout_chip_name() const { return info_.part_number.data(); }
#endif
