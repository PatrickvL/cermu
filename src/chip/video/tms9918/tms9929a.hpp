#pragma once
/*
 * tms9929a.hpp — TMS9929A revised PAL VDP
 *
 * Used in: Tatung Einstein, PAL MSX1.
 * PAL timing (313 lines), composite video output.
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits TMS9929ATraits = {
    "Texas Instruments",             // vendor
    "TMS9929A",                      // chip_id
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

using TMS9929A = tms9918_t<TMS9929ATraits>;

} // namespace tms9918

using TMS9929A = tms9918::TMS9929A;
