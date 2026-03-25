#pragma once
/*
 * sega_315_5246.hpp — Sega 315-5246 VDP (Master System PAL / Game Gear)
 *
 * PAL variant of the Sega VDP with minor register differences
 * from the 315-5124. Also used as the basis for Game Gear VDP.
 * PAL timing (313 lines at ~50 Hz).
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits SEGA_315_5246Traits = {
    "Sega",                          // vendor
    "315-5246",                      // chip_id
    "Sega 315-5246",                 // display_name
    VDPRegion::PAL,                  // region
    VDPOutput::COMPOSITE,            // output
    VDPSpriteModel::SEGA,            // sprite_model
    VDPPaletteModel::FIXED_15,       // palette_model
    VDPScrollModel::SEGA,            // scroll_model
    VDPFeatureFlags::SEGA_MODE_EXT,  // feature_flags
    11,                              // num_registers
    16,                              // vram_size_kb
    313,                             // total_lines
    192,                             // visible_lines
    5'320'000,                       // dot_clock_hz
};

using SEGA_315_5246 = tms9918_t<SEGA_315_5246Traits>;

} // namespace tms9918

using SEGA_315_5246 = tms9918::SEGA_315_5246;
