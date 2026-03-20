#pragma once
/*
 * c014795.hpp — Atari POKEY C014795 (5200 / arcade variant)
 *
 * POKEY variant for the Atari 5200 SuperSystem console and certain
 * arcade boards. Retains pot inputs and keyboard scanning but is
 * typically used with the 5200 keypad/controller rather than the
 * full 400/800-style keyboard matrix.
 */

#include "chip/sound/pokey/pokey.hpp"

namespace pokey {

inline constexpr POKEYTraits C014795_Traits = {
    "Atari",                     // vendor
    "C014795",                   // chip_id
    CoreFlags::NMOS_5200,        // core_flags — 5200 context
    40                           // pin_count
};

using C014795 = pokey_t<C014795_Traits>;

} // namespace pokey

using AtariPOKEY_5200 = pokey::C014795;
