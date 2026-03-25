#pragma once
/*
 * ym3438.hpp — Yamaha YM3438 (OPN2C) type alias
 *
 * CMOS version of the YM2612.  Functionally identical, but eliminates
 * the "ladder effect" DAC distortion present in early YM2612 revisions.
 * Used in later Mega Drive / Genesis revisions (model 2+).
 */

#include "chip/sound/ym_fm/ym_fm.hpp"

inline constexpr YMTraits YM3438_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM3438",
    .display_name         = "Yamaha YM3438 (OPN2C)",
    .fm_channels          = 6,
    .operators_per_channel = 4,
    .fm_algorithms        = 8,
    .has_ch3_special_mode = true,
    .has_lfo              = true,
    .has_rhythm_mode      = false,
    .has_waveform_select  = false,
    .has_rom_patches      = false,
    .has_ssg              = false,
    .ssg                  = nullptr,
    .has_adpcm_a          = false,
    .has_adpcm_b          = false,
    .has_dac              = true,
    .pin_count            = 24,
};

using YM3438 = ym_fm_t<YM3438_Traits>;
