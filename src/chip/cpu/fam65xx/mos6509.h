#pragma once
/*
 * mos6509.h — MOS 6509 CPU type (separation layer)
 *
 * Defines the MOS 6509 CPU trait.  The 6509 extends the 6502 with a
 * 20-bit address space via MOS6509-style banking (used in the CBM-II).
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits MOS6509Traits = {
    "MOS Technology",                                 // vendor
    "6509",                                           // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_BANKING, // core_flags
    20,                                               // address_bits
    0x00,                                             // io_port_mask
    BankingType::MOS6509,                             // banking
    {SoundChip::NONE, DMAController::NONE, false}     // peripheral
};

} // namespace fam65xx

// Re-export outside fam65xx namespace for convenience
inline constexpr auto& MOS6509Traits = fam65xx::MOS6509Traits;
