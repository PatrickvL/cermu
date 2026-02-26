#pragma once
/*
 * mos6507.h — MOS 6507 CPU type (separation layer)
 *
 * Defines the MOS 6507 CPU trait.  The 6507 is a pin-reduced 6502 with
 * only 13 address lines and no IRQ line (used in the Atari 2600).
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits MOS6507Traits = {
    "MOS Technology",                                 // vendor
    "6507",                                           // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::NO_IRQ_LINE, // core_flags
    13,                                               // address_bits
    0x00,                                             // io_port_mask
    BankingType::NONE,                                // banking
    {SoundChip::NONE, DMAController::NONE, false}     // peripheral
};

} // namespace fam65xx

// Trait constants remain internal to namespace fam65xx.
// No type alias — the 6507 is not yet instantiated as a distinct type.
