#pragma once
/*
 * c012294.hpp — Atari POKEY C012294 (original 1979 NMOS)
 *
 * The original POKEY as used in the Atari 400/800 home computers and
 * many arcade boards (Asteroids Deluxe, Centipede, Tempest, etc.).
 * 40-pin DIP, NMOS process.
 */

#include "chip/sound/pokey/pokey.hpp"

namespace pokey {

inline constexpr POKEYTraits C012294_Traits = {
    "Atari",                     // vendor
    "C012294",                   // chip_id
    "Atari POKEY",               // display_name
    CoreFlags::NMOS_FULL,        // core_flags — full home computer config
    40                           // pin_count
};

using C012294 = pokey_t<C012294_Traits>;

} // namespace pokey

using AtariPOKEY = pokey::C012294;
