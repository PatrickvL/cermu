#pragma once
/*
 * ay_3_8913.hpp — AY-3-8913 type alias (no I/O ports, 24-pin DIP)
 *
 * Compact embed variant with no I/O ports — sound generation only.
 */

#include "chip/sound/ay_psg/ay_psg.hpp"

inline constexpr AYTraits AY_3_8913_Traits = {
    "General Instrument", "AY-3-8913",
    0, 2, 16, AYRegisterMap::STANDARD, false
};

using AY_3_8913 = ay_psg_t<AY_3_8913_Traits>;
