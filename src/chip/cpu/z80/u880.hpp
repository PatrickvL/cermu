#pragma once
/*
 * u880.h — VEB MME U880 CPU type (Z80A clone, DDR)
 *
 * The U880 was the East German (DDR) clone of the Zilog Z80A, manufactured
 * by VEB Mikroelektronik "Karl Marx" Erfurt (MME).  Fully compatible with
 * the Z80A at 4 MHz.  Pin-for-pin replacement.
 *
 * Used in: LC80, Z1013, Z9001/KC87, KC85 series, and virtually all
 * DDR home/educational computers.
 *
 * 40-pin DIP package.
 */

#include "chip/cpu/z80/z80.hpp"

namespace z80 {

inline constexpr Z80Traits U880Traits = {
    "VEB MME Erfurt",                                    // vendor
    "U880",                                              // chip_id
    "U880",                                              // display_name
    CoreFlags::NMOS_Z80,                                 // core_flags (Z80A compatible)
    16,                                                  // address_bits
    40,                                                  // max_clock_mhz_x10 (4.0 MHz)
    {Z80SoundChip::NONE, Z80DMAType::NONE, false, false} // peripheral
};

using U880 = z80_t<U880Traits>;

} // namespace z80

using U880 = z80::U880;
