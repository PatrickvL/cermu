#pragma once
/*
 * ym2413.hpp — Yamaha YM2413 (OPLL) type alias
 *
 * 9-channel 2-operator FM with 15 ROM instrument patches + 1 user patch.
 * Optional rhythm mode converts channels 7-8-9 into 5 percussion voices.
 * Used in MSX-MUSIC, Sega Master System (Japanese), and arcade boards.
 */

#include "chip/sound/ym_fm/ym_fm.hpp"

inline constexpr YMTraits YM2413_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM2413",
    .display_name         = "Yamaha YM2413 (OPLL)",
    .fm_channels          = 9,
    .operators_per_channel = 2,
    .fm_algorithms        = 4,
    .has_ch3_special_mode = false,
    .has_lfo              = false,
    .has_rhythm_mode      = true,
    .has_waveform_select  = false,
    .has_rom_patches      = true,
    .has_ssg              = false,
    .ssg                  = nullptr,
    .has_adpcm_a          = false,
    .has_adpcm_b          = false,
    .has_dac              = false,
    .pin_count            = 18,
};

using YM2413 = ym_fm_t<YM2413_Traits>;
