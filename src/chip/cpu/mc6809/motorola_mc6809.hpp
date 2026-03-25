#pragma once
#include "chip/cpu/mc6809/mc6809.hpp"

namespace mc6809 {

inline constexpr MC6809Traits MotorolaMC6809Traits = {
    "Motorola",                   // vendor
    "MC6809",                     // chip_id
    "Motorola MC6809",            // display_name
    CoreFlags::MC6809_BASE,       // core_flags
    16,                           // address_bits
    10,                           // max_clock_mhz_x10 (1.0 MHz)
};

using MotorolaMC6809 = mc6809_t<MotorolaMC6809Traits>;

} // namespace mc6809

using MotorolaMC6809 = mc6809::MotorolaMC6809;
