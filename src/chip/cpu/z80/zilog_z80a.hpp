#pragma once
/*
 * zilog_z80a.h — Zilog Z80A CPU type (NMOS, 4.0 MHz)
 *
 * The Z80A (1978) is the most common Z80 variant, rated for 4 MHz.
 * Identical instruction set and behavior to the original Z80, just faster.
 * Used in: ZX Spectrum 48K/128K, Amstrad CPC, MSX, CP/M machines,
 * Pac-Man and many other arcade machines, DDR home computers.
 *
 * 40-pin DIP package.
 */

#include "chip/cpu/z80/z80.hpp"

namespace z80 {

inline constexpr Z80Traits ZilogZ80ATraits = {
    "Zilog",                                             // vendor
    "Z80A",                                              // chip_id
    "Zilog Z80A",                                        // display_name
    CoreFlags::NMOS_Z80,                                 // core_flags
    16,                                                  // address_bits
    40,                                                  // max_clock_mhz_x10 (4.0 MHz)
    {Z80SoundChip::NONE, Z80DMAType::NONE, false, false} // peripheral
};

using ZilogZ80A = z80_t<ZilogZ80ATraits>;

} // namespace z80

using ZilogZ80A = z80::ZilogZ80A;
