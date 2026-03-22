#pragma once
#include "chip/cpu/mc6809/mc6809.hpp"

namespace mc6809 {

inline constexpr MC6809Traits MotorolaMC6809ETtraits = {
    "Motorola",                   // vendor
    "MC6809E",                    // chip_id
    CoreFlags::MC6809E_BASE,      // core_flags
    16,                           // address_bits
    10,                           // max_clock_mhz_x10 (1.0 MHz)
};

using MotorolaMC6809E = mc6809_t<MotorolaMC6809ETtraits>;

} // namespace mc6809

using MotorolaMC6809E = mc6809::MotorolaMC6809E;
