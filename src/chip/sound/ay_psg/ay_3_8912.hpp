#pragma once
/*
 * ay_3_8912.hpp — AY-3-8912 type alias (1 I/O port, 28-pin DIP)
 *
 * Reduced pin-count variant of the AY-3-8910 with one I/O port removed.
 * Used in ZX Spectrum 128K, Amstrad CPC, and various systems.
 */

#include "chip/sound/ay_psg/ay_psg.hpp"

inline constexpr AYTraits AY_3_8912_Traits = {
    "General Instrument", "AY-3-8912", "GI AY-3-8912",
    1, 2, 16, AYRegisterMap::STANDARD, false
};

using AY_3_8912 = ay_psg_t<AY_3_8912_Traits>;
