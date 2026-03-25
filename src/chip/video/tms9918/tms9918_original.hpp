#pragma once
/*
 * tms9918.hpp — TMS9918 original NTSC VDP (TI-99/4)
 *
 * Original chip with composite video output.
 * Identical to TMS9918A except for minor timing differences
 * (TMS9918 has a slightly different interrupt latch behavior).
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits TMS9918Traits = {
    "Texas Instruments",             // vendor
    "TMS9918",                       // chip_id
    "TI TMS9918",                    // display_name
    VDPRegion::NTSC,                 // region
    VDPOutput::COMPOSITE,            // output
    VDPSpriteModel::ORIGINAL,        // sprite_model
    VDPPaletteModel::FIXED_15,       // palette_model
    VDPScrollModel::NONE,            // scroll_model
    0,                               // feature_flags
    8,                               // num_registers
    16,                              // vram_size_kb
    262,                             // total_lines
    192,                             // visible_lines
    5'370'000,                       // dot_clock_hz
};

using TMS9918 = tms9918_t<TMS9918Traits>;

} // namespace tms9918

using TMS9918 = tms9918::TMS9918;
