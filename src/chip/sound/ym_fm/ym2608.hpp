#pragma once
/*
 * ym2608.hpp — Yamaha YM2608 (OPNA) type alias
 *
 * 6-channel 4-operator FM + embedded SSG (YM2149 equivalent)
 * + ADPCM-A (6-channel, rhythm samples) + ADPCM-B (1-channel, streaming).
 * Used in NEC PC-8801mkIISR, PC-9801-26K/86, and arcade boards.
 */

#include "chip/sound/ym_fm/ym_fm.hpp"
#include "chip/sound/ay_psg/ym2149.hpp"   // SSG traits

inline constexpr YMTraits YM2608_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM2608",
    .fm_channels          = 6,
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
    .pin_count            = 64,
};

using YM2608 = ym_fm_t<YM2608_Traits>;
