#pragma once
/*
 * c012294b.hpp — Atari POKEY C012294B (revised NMOS)
 *
 * Minor revision of the original POKEY with timer-related bug fix.
 * Same pinout, same feature set. Used in later Atari 800XL/130XE runs
 * and some arcade boards.
 */

#include "chip/sound/pokey/pokey.hpp"

namespace pokey {

inline constexpr POKEYTraits C012294B_Traits = {
    "Atari",                     // vendor
    "C012294B",                  // chip_id
    "Atari POKEY",               // display_name
    CoreFlags::NMOS_B,           // core_flags — timer bug fix variant
    40                           // pin_count
};

using C012294B = pokey_t<C012294B_Traits>;

} // namespace pokey

using AtariPOKEY_B = pokey::C012294B;
