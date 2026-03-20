#pragma once
/*
 * ym3812.hpp — Yamaha YM3812 (OPL2) type alias
 *
 * 9-channel 2-operator FM with rhythm mode and selectable waveforms
 * (4 sine variants per operator).  Used in AdLib and Sound Blaster
 * PC sound cards, and many arcade boards.
 */

#include "chip/sound/ym_fm/ym_fm.hpp"

inline constexpr YMTraits YM3812_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM3812",
    .fm_channels          = 9,
    .operators_per_channel = 2,
    .fm_algorithms        = 4,
    .has_ch3_special_mode = false,
    .has_lfo              = false,
    .has_rhythm_mode      = true,
    .has_waveform_select  = true,
    .has_rom_patches      = false,
    .has_ssg              = false,
    .ssg                  = nullptr,
    .has_adpcm_a          = false,
    .has_adpcm_b          = false,
    .has_dac              = false,
    .pin_count            = 24,
};

using YM3812 = ym_fm_t<YM3812_Traits>;
