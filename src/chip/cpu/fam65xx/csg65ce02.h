#pragma once
/*
 * csg65ce02.h — CSG 65CE02 CPU type (separation layer)
 *
 * Defines the CSG 65CE02 CPU trait.  The 65CE02 is Commodore's extended
 * CMOS 6502 with extra instructions, optimized cycle timings, and
 * additional addressing modes (precursor to the 4510).
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits CSG_65CE02Traits = {
    "Commodore", // vendor
    "65CE02",    // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::CE02_EXTENDED |
        CPUCoreFlags::OPTIMIZED_CYCLES,           // core_flags
    16,                                           // address_bits
    0x00,                                         // io_port_mask
    BankingType::NONE,                            // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

} // namespace fam65xx

// Re-export outside fam65xx namespace for convenience
inline constexpr auto& CSG_65CE02Traits = fam65xx::CSG_65CE02Traits;
