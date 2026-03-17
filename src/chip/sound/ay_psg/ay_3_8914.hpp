#pragma once
/*
 * ay_3_8914.hpp — AY-3-8914 type alias (reshuffled register map)
 *
 * Used in the Mattel Intellivision.  Pin-compatible with the AY-3-8910
 * but the register addresses are rearranged so the mixer/control register
 * sits at address 0 instead of 7.
 */

#include "chip/sound/ay_psg/ay_psg.hpp"

using AY_3_8914 = ay_psg_t<AY_3_8914_Traits>;
