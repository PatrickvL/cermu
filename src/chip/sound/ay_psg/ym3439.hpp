#pragma once
/*
 * ym3439.hpp — Yamaha YM3439 type alias
 *
 * CMOS version of the YM2149.  Functionally identical with lower power
 * consumption.  Used in some late MSX machines and arcade boards.
 */

#include "chip/sound/ay_psg/ay_psg.hpp"

inline constexpr AYTraits YM3439_Traits = {
    "Yamaha", "YM3439", "Yamaha YM3439",
    2, 1, 32, AYRegisterMap::STANDARD, false
};

using YM3439 = ay_psg_t<YM3439_Traits>;
