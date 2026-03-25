#pragma once
/*
 * ym3526.hpp — Yamaha YM3526 (OPL) type alias
 *
 * 9-channel 2-operator FM with rhythm mode.  The original OPL.
 * Used in the MSX-AUDIO cartridge, arcade boards (Bubble Bobble,
 * Double Dragon), and early AdLib/Sound Blaster PC cards (pre-OPL2).
 */

#include "chip/sound/ym_fm/ym_fm.hpp"

inline constexpr YMTraits YM3526_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM3526",
    .display_name         = "Yamaha YM3526 (OPL)",
    .fm_channels          = 9,
    .operators_per_channel = 2,
    .fm_algorithms        = 4,
    .has_ch3_special_mode = false,
    .has_lfo              = false,
    .has_rhythm_mode      = true,
    .has_waveform_select  = false,
    .has_rom_patches      = false,
    .has_ssg              = false,
    .ssg                  = nullptr,
    .has_adpcm_a          = false,
    .has_adpcm_b          = false,
    .has_dac              = false,
    .pin_count            = 24,
};

using YM3526 = ym_fm_t<YM3526_Traits>;
