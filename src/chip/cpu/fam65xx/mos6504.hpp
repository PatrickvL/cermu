#pragma once
/*
 * mos6504.h — MOS 6504 CPU type (separation layer)
 *
 * Defines the MOS 6504 CPU trait.  The 6504 is a pin-reduced 6502 with
 * only 13 address lines (8 KB address space) and fewer package pins.
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "chip/cpu/fam65xx/fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits MOS6504Traits = {
    "MOS Technology",                             // vendor
    "6504",                                       // chip_id
    "MOS 6504",                                   // display_name
    CoreFlags::NMOS_BASE,                         // core_flags
    13,                                           // address_bits
    0x00,                                         // io_port_mask
    BankingType::NONE,                            // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

using MOS6504 = fam65xx_t<MOS6504Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using MOS6504 = fam65xx::MOS6504;
