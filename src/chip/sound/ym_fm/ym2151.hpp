#pragma once
/*
 * ym2151.hpp — Yamaha YM2151 (OPM) type alias
 *
 * 8-channel, 4-operator FM synthesizer.  The first standalone FM chip
 * from Yamaha.  No SSG, no ADPCM.  Used in arcade machines (Sega,
 * Konami, Capcom), Sharp X1, Sharp X68000, and many synthesizers.
 */

#include "chip/sound/ym_fm/ym_fm.hpp"

inline constexpr YMTraits YM2151_Traits = {
    .vendor               = "Yamaha",
    .chip_id              = "YM2151",
    .display_name         = "Yamaha YM2151 (OPM)",
    .fm_channels          = 8,
    .operators_per_channel = 4,
    .fm_algorithms        = 8,
    .has_ch3_special_mode = false,
    .has_lfo              = true,
    .has_rhythm_mode      = false,
    .has_waveform_select  = false,
    .has_rom_patches      = false,
    .has_ssg              = false,
    .ssg                  = nullptr,
    .has_adpcm_a          = false,
    .has_adpcm_b          = false,
    .has_dac              = false,
    .ladder_effect        = false,
    .pin_count            = 24,
};

using YM2151 = ym_fm_t<YM2151_Traits>;
