#pragma once
#include "chip/cpu/mc6809/mc6809.hpp"

namespace mc6809 {

inline constexpr MC6809Traits HitachiHD6309Traits = {
    "Hitachi",                    // vendor
    "HD6309",                     // chip_id
    "Hitachi HD6309",             // display_name
    CoreFlags::HD6309_BASE,       // core_flags
    16,                           // address_bits
    20,                           // max_clock_mhz_x10 (2.0 MHz)
};

using HitachiHD6309 = mc6809_t<HitachiHD6309Traits>;

} // namespace mc6809

using HitachiHD6309 = mc6809::HitachiHD6309;
