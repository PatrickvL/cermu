#pragma once
/*
 * mos6561.hpp — MOS6561 PAL VIC-I type alias
 *
 * PAL variant of the VIC-I chip.  Used in PAL VIC-20 models.
 * PAL crystal: 4.433619 MHz / 4 = 1,108,405 Hz system clock.
 * 63 cycles per line, 312 total lines per frame → ~50 Hz refresh.
 *
 * NOTE: VICE uses 71 cycles/line for PAL.  63 here matches the existing
 * working rendering code and should be reviewed separately.
 */

#include "chip/video/vic/vic_common.hpp"

inline constexpr VicTraits MOS6561_traits = {
    .chip_name       = "MOS6561 PAL",
    .chip_id         = "MOS6561",
    .vendor          = "MOS Technology",
    .cycles_per_line = 63,
    .total_lines     = 312,
    .clock_frequency = 1108405,
    .is_pal          = true,
};

using mos6561_t = vic_t<MOS6561_traits>;