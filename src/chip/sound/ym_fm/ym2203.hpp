#pragma once
/*
 * ym2203.hpp — Yamaha YM2203 (OPN) type alias
 *
 * 3-channel 4-operator FM + embedded SSG (YM2149 equivalent).
 * The first OPN-family chip.  Used in NEC PC-8801, PC-9801,
 * Sharp X1 turbo, FM-7, and many arcade boards.
 */

#include "chip/sound/ym_fm/ym_fm.hpp"
#include "chip/sound/ay_psg/ym2149.hpp"   // SSG traits

inline constexpr YMTraits YM2203_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM2203",
    .display_name         = "Yamaha YM2203 (OPN)",
    .fm_channels          = 3,
    .operators_per_channel = 4,
    .fm_algorithms        = 8,
    .has_ch3_special_mode = true,
    .has_lfo              = false,
    .has_rhythm_mode      = false,
    .has_waveform_select  = false,
    .has_rom_patches      = false,
    .has_ssg              = true,
    .ssg                  = &YM2149_Traits,
    .has_adpcm_a          = false,
    .has_adpcm_b          = false,
    .has_dac              = false,
    .pin_count            = 40,
};

using YM2203 = ym_fm_t<YM2203_Traits>;
