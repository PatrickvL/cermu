#pragma once
/*
 * ym2612.hpp — Yamaha YM2612 (OPN2) type alias
 *
 * 6-channel 4-operator FM with DAC mode on channel 6.
 * No SSG block.  The sound chip of the Sega Mega Drive / Genesis.
 */

#include "chip/sound/ym_fm/ym_fm.hpp"

inline constexpr YMTraits YM2612_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM2612",
    .display_name         = "Yamaha YM2612 (OPN2)",
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

using YM2612 = ym_fm_t<YM2612_Traits>;
