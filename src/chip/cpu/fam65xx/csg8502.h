#pragma once
/*
 * csg8502.h — CSG 8502 CPU type (separation layer)
 *
 * Defines the CSG 8502 CPU trait.  The 8502 is the C128 CPU — an
 * NMOS 65xx with an I/O port, variable clock (1/2 MHz switching),
 * and a 7-bit I/O port mask.
 * No dedicated pin layout yet — falls through to generic DIP fallback.
 */

#include "fam65xx.hpp"

namespace fam65xx {

inline constexpr CPUTraits CSG8502Traits = {
    "Commodore", // vendor
    "CSG8502",   // chip_id
    CoreFlags::NMOS_BASE | CPUCoreFlags::HAS_IO_PORT |
        CPUCoreFlags::VARIABLE_CLOCK, // core_flags
    16,                               // address_bits
    0x7F,                             // io_port_mask (Pins 0-6, no pin 7)
    BankingType::NONE,                // banking
    {SoundChip::NONE, DMAController::NONE, false} // peripheral
};

} // namespace fam65xx

// Re-export outside fam65xx namespace for convenience
inline constexpr auto& CSG8502Traits = fam65xx::CSG8502Traits;
