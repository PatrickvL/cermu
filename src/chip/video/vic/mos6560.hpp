#pragma once
/*
 * mos6560.hpp — MOS6560 NTSC VIC-I type alias
 *
 * NTSC variant of the VIC-I chip.  Used in NTSC VIC-20 models.
 * NTSC crystal: 14.31818 MHz / 14 = 1,022,727 Hz system clock.
 * 65 cycles per line, 262 total lines per frame → ~60 Hz refresh.
 */

#include "chip/video/vic/vic_common.hpp"

inline constexpr VicTraits MOS6560_traits = {
    .chip_name       = "MOS6560 NTSC",
    .chip_id         = "MOS6560",
    .vendor          = "MOS Technology",
    .cycles_per_line = 65,
    .total_lines     = 262,
    .clock_frequency = 1022727,
    .is_pal          = false,
};

using mos6560_t = vic_t<MOS6560_traits>;