#pragma once
/*
 * ym2149.hpp — Yamaha YM2149 SSG type alias
 *
 * Yamaha-licensed clone of the AY-3-8910 with half-step envelope
 * precision (32 envelope steps vs. 16) and no internal ÷2 clock divider.
 * Used in MSX, Atari ST, and alongside Yamaha FM chips (OPN series).
 */

#include "chip/sound/ay_psg/ay_psg.hpp"

inline constexpr AYTraits YM2149_Traits = {
    "Yamaha", "YM2149",
    2, 1, 32, AYRegisterMap::STANDARD, false
};

using YM2149 = ay_psg_t<YM2149_Traits>;
