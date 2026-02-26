#pragma once
/*
 * mos6504.h — MOS 6504 CPU type (separation layer)
 *
 * Defines the MOS 6504 CPU trait.  The 6504 is a pin-reduced 6502 with
 * only 13 address lines (8 KB address space) and fewer package pins.
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits MOS6504Traits = {
    "MOS Technology",                             // vendor
    "6504",                                       // chip_id
    CoreFlags::NMOS_BASE,                         // core_flags
    13,                                           // address_bits
    0x00,                                         // io_port_mask
    BankingType::NONE,                            // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

} // namespace fam65xx

// Re-export outside fam65xx namespace for convenience
inline constexpr auto& MOS6504Traits = fam65xx::MOS6504Traits;
