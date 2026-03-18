#pragma once
/*
 * ay_3_8910.hpp — AY-3-8910 type alias (2 I/O ports, 40-pin DIP)
 *
 * General Instrument AY-3-8910: the original PSG.
 * Used in MSX1, Amstrad CPC, ZX Spectrum 128, many arcade machines.
 */

#include "chip/sound/ay_psg/ay_psg.hpp"

inline constexpr AYTraits AY_3_8910_Traits = {
    "General Instrument", "AY-3-8910",
    2, 2, 16, AYRegisterMap::STANDARD, false
};

using AY_3_8910 = ay_psg_t<AY_3_8910_Traits>;
