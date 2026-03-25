#pragma once
/*
 * tms9929.hpp — TMS9929 PAL VDP (original)
 *
 * PAL version of TMS9918. 313 lines per frame at ~50 Hz.
 * Composite video output.
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits TMS9929Traits = {
    "Texas Instruments",             // vendor
    "TMS9929",                       // chip_id
    "TI TMS9929",                    // display_name
    VDPRegion::PAL,                  // region
    VDPOutput::COMPOSITE,            // output
    VDPSpriteModel::ORIGINAL,        // sprite_model
    VDPPaletteModel::FIXED_15,       // palette_model
    VDPScrollModel::NONE,            // scroll_model
    0,                               // feature_flags
    8,                               // num_registers
    16,                              // vram_size_kb
    313,                             // total_lines
    192,                             // visible_lines
    5'320'000,                       // dot_clock_hz
};

using TMS9929 = tms9918_t<TMS9929Traits>;

} // namespace tms9918

using TMS9929 = tms9918::TMS9929;
