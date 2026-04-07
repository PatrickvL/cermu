#pragma once
/*
 * ym2610.hpp — Yamaha YM2610 (OPNB) type alias
 *
 * 4-channel 4-operator FM + embedded SSG (YM2149 equivalent)
 * + ADPCM-A (6-channel) + ADPCM-B (1-channel).
 * The sound chip of the SNK Neo Geo (MVS and AES).
 */

#include "chip/sound/ym_fm/ym_fm.hpp"
#include "chip/sound/ay_psg/ym2149.hpp"   // SSG traits

inline constexpr YMTraits YM2610_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM2610",
    .display_name         = "Yamaha YM2610 (OPNB)",
    .fm_channels          = 4,
    .operators_per_channel = 4,
    .fm_algorithms        = 8,
    .has_ch3_special_mode = true,
    .has_lfo              = true,
    .has_rhythm_mode      = false,
    .has_waveform_select  = false,
    .has_rom_patches      = false,
    .has_ssg              = true,
    .ssg                  = &YM2149_Traits,
    .has_adpcm_a          = true,
    .has_adpcm_b          = true,
    .has_dac              = false,
    .ladder_effect        = false,
    .pin_count            = 64,
};

using YM2610 = ym_fm_t<YM2610_Traits>;
