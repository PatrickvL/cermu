#pragma once
/*
 * tms9918a.hpp — TMS9918A revised NTSC VDP
 *
 * Used in: ColecoVision, MSX1 (NTSC composite), SG-1000, TI-99/4A.
 * Composite video output, NTSC timing (262 lines).
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits TMS9918ATraits = {
    "Texas Instruments",             // vendor
    "TMS9918A",                      // chip_id
    "TI TMS9918A",                   // display_name
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

using TMS9918A = tms9918_t<TMS9918ATraits>;

} // namespace tms9918

using TMS9918A = tms9918::TMS9918A;
