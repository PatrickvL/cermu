#pragma once
/*
 * zilog_z80.h — Zilog Z80 CPU type (original NMOS, 2.5 MHz)
 *
 * Defines the original Zilog Z80 (1976) trait, type alias, and pin layout.
 * The Z80 is a binary-compatible superset of the Intel 8080 with added
 * index registers (IX/IY), shadow register set, block I/O, bit manipulation,
 * relative jumps, and a built-in DRAM refresh counter.
 *
 * 40-pin DIP package.
 */

#include "chip/cpu/z80/z80.hpp"

namespace z80 {

inline constexpr Z80Traits ZilogZ80Traits = {
    "Zilog",                                             // vendor
    "Z80",                                               // chip_id
    CoreFlags::NMOS_Z80,                                 // core_flags
    16,                                                  // address_bits
    25,                                                  // max_clock_mhz_x10 (2.5 MHz)
    {Z80SoundChip::NONE, Z80DMAType::NONE, false, false} // peripheral
};

using ZilogZ80 = z80_t<ZilogZ80Traits>;

} // namespace z80

using ZilogZ80 = z80::ZilogZ80;
