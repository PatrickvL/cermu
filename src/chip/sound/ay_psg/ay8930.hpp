#pragma once
/*
 * ay8930.hpp — Microchip AY8930 type alias
 *
 * Enhanced AY-3-8910 clone with per-channel envelope generators,
 * extended noise period (8-bit), duty cycle control, and bank-switched
 * extended register set.  Rare in practice; used in some arcade and
 * homebrew hardware.
 *
 * Extended mode features implemented:
 *   - Mode activation via register $0D bit 4
 *   - Bank B register file for per-channel configuration
 *   - Per-channel independent envelope generators (period + shape)
 *   - Variable duty cycle per channel (4-bit, default 50%)
 */

#include "chip/sound/ay_psg/ay_psg.hpp"

inline constexpr AYTraits AY8930_Traits = {
    "Microchip", "AY8930", "Microchip AY8930",
    2, 2, 16, AYRegisterMap::STANDARD, true
};

using AY8930 = ay_psg_t<AY8930_Traits>;
