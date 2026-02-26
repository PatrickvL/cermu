#pragma once
/*
 * synertek65c02.h — Synertek 65C02 CPU type (separation layer)
 *
 * Defines the Synertek 65C02 CPU trait and type alias.
 * No dedicated pin layout — falls through to the generic DIP-40 fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

// Synertek 65C02 — CMOS base (same flags as early WDC, different vendor)
inline constexpr CPUTraits SYNERTEK_65C02Traits = {
    "Synertek",                                   // vendor
    "65C02",                                      // chip_id
    CoreFlags::CMOS_BASE_FLAGS,                   // core_flags
    16,                                           // address_bits
    0x00,                                         // io_port_mask
    BankingType::NONE,                            // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

using SYNERTEK_65C02 = fam65xx_t<SYNERTEK_65C02Traits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using SYNERTEK_65C02 = fam65xx::SYNERTEK_65C02;
