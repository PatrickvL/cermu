#pragma once
/*
 * hudson_huc6280.h — Hudson HuC6280 CPU type (separation layer)
 *
 * Defines the Hudson Soft HuC6280 CPU trait.  The HuC6280 is the
 * TurboGrafx-16/PC Engine CPU — a CMOS 65xx with HuC6280 extensions,
 * variable clock, HuC6280-style banking, and an integrated PSG.
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits HUDSON_HUC6280Traits = {
    "Hudson Soft", // vendor
    "HuC6280",     // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::HUC6280_EXTENDED |
        CPUCoreFlags::VARIABLE_CLOCK | CPUCoreFlags::HAS_BANKING, // core_flags
    21,                                                 // address_bits
    0x00,                                               // io_port_mask
    BankingType::HUC6280,                               // banking
    {SoundChip::HUC6280_PSG, DMAController::NONE, true} // peripheral
};

} // namespace fam65xx

// Trait constants remain internal to namespace fam65xx.
// No type alias — the HuC6280 is not yet instantiated as a distinct type.
