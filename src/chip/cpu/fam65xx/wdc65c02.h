#pragma once
/*
 * wdc65c02.h — WDC 65C02 (early) CPU type (separation layer)
 *
 * Defines the early WDC 65C02 trait plus its rebrand aliases (WDC 65SC02,
 * GTE G65SC02), and the type alias.  No dedicated pin layout — the early
 * 65C02 falls through to the generic 40-pin DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

// WDC 65C02 (early) — CMOS base without Rockwell/WAI/STP extensions
inline constexpr CPUTraits WDC_65C02_EARLYTraits = {
    "WDC",                                        // vendor
    "65C02",                                      // chip_id
    CoreFlags::CMOS_BASE_FLAGS,                   // core_flags
    16,                                           // address_bits
    0x00,                                         // io_port_mask
    BankingType::NONE,                            // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

inline constexpr CPUTraits WDC_65SC02Traits = WDC_65C02_EARLYTraits;
inline constexpr CPUTraits GTE_G65SC02Traits = WDC_65C02_EARLYTraits;

using WDC_65C02 = fam65xx_t<WDC_65C02_EARLYTraits>;

} // namespace fam65xx

// Re-export type alias outside fam65xx namespace for convenience.
// Trait constants remain internal to namespace fam65xx.
using WDC_65C02 = fam65xx::WDC_65C02;
