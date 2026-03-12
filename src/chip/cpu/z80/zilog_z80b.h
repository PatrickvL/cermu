#pragma once
/*
 * zilog_z80b.h — Zilog Z80B CPU type (NMOS, 6.0 MHz)
 *
 * Higher-speed variant (1980).  Sometimes found in upgraded Z80 systems
 * and some arcade boards.
 *
 * 40-pin DIP package.
 */

#include "chip/cpu/z80/z80.hpp"

namespace z80 {

inline constexpr Z80Traits ZilogZ80BTraits = {
    "Zilog",                                             // vendor
    "Z80B",                                              // chip_id
    CoreFlags::NMOS_Z80,                                 // core_flags
    16,                                                  // address_bits
    60,                                                  // max_clock_mhz_x10 (6.0 MHz)
    {Z80SoundChip::NONE, Z80DMAType::NONE, false, false} // peripheral
};

using ZilogZ80B = z80_t<ZilogZ80BTraits>;

} // namespace z80

using ZilogZ80B = z80::ZilogZ80B;
