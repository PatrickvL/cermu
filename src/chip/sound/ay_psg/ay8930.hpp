#pragma once
/*
 * ay8930.hpp — Microchip AY8930 type alias
 *
 * Enhanced AY-3-8910 clone with per-channel envelope generators,
 * extended noise period (8-bit), duty cycle control, and bank-switched
 * extended register set.  Rare in practice; used in some arcade and
 * homebrew hardware.
 *
 * NOTE: Extended-mode behaviour (per-channel envelope, duty cycle,
 * bank register) is not yet implemented.  The chip currently operates
 * in standard AY-3-8910 compatibility mode.
 */

#include "chip/sound/ay_psg/ay_psg.hpp"

inline constexpr AYTraits AY8930_Traits = {
    "Microchip", "AY8930", "Microchip AY8930",
    2, 2, 16, AYRegisterMap::STANDARD, true
};

using AY8930 = ay_psg_t<AY8930_Traits>;
