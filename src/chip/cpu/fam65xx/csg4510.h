#pragma once
/*
 * csg4510.h — CSG 4510 CPU type (separation layer)
 *
 * Defines the CSG 4510 CPU trait.  The 4510 is the C65 CPU — a 65CE02
 * with 20-bit addressing via CSG4510-style banking and an integrated
 * DMA controller.
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits CSG_4510Traits = {
    "Commodore", // vendor
    "4510",      // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::CE02_EXTENDED |
        CPUCoreFlags::OPTIMIZED_CYCLES |
        CPUCoreFlags::HAS_BANKING,                       // core_flags
    20,                                                  // address_bits
    0x00,                                                // io_port_mask
    BankingType::CSG4510,                                // banking
    {SoundChip::NONE, DMAController::CSG4510_DMA, false} // peripheral
};

} // namespace fam65xx

// Trait constants remain internal to namespace fam65xx.
// No type alias — the 4510 is not yet instantiated as a distinct type.
