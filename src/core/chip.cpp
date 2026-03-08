#include "chip.h"

#ifdef CERMU_HAS_GUI
#include "chip_layout.h"

std::vector<PinSignalState> ChipBase::get_layout_pin_states(ChipLayout&) { return {}; }
const char* ChipBase::get_layout_chip_name() const { return info_.part_number.data(); }
#endif
