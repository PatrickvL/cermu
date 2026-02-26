#pragma once
/*
 * ricoh_5a22.h — Ricoh 5A22 CPU type (separation layer)
 *
 * Defines the Ricoh 5A22 CPU trait.  The 5A22 is the SNES CPU — a
 * 65C816-based 16-bit processor with an integrated DMA controller.
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits RICOH_5A22Traits = {
    "Ricoh",                                                // vendor
    "5A22",                                                 // chip_id
    CoreFlags::CMOS_BASE_FLAGS | CPUCoreFlags::C816_16BIT,  // core_flags
    24,                                                     // address_bits
    0x00,                                                   // io_port_mask
    BankingType::NONE,                                      // banking
    {SoundChip::NONE, DMAController::RICOH_5A22_DMA, false} // peripheral
};

} // namespace fam65xx

// Trait constants remain internal to namespace fam65xx.
// No type alias — the 5A22 is not yet instantiated as a distinct type.
