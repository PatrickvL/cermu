#pragma once
/*
 * tms9928a.hpp — TMS9928A NTSC RGB VDP
 *
 * Used in: some MSX1 boards (RGB monitor output).
 * Separate R/G/B/Y analog outputs instead of composite encoder.
 * NTSC timing (262 lines).
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits TMS9928ATraits = {
    "Texas Instruments",             // vendor
    "TMS9928A",                      // chip_id
    "TI TMS9928A",                   // display_name
    VDPRegion::NTSC,                 // region
    VDPOutput::RGB,                  // output
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

using TMS9928A = tms9918_t<TMS9928ATraits>;

} // namespace tms9918

using TMS9928A = tms9918::TMS9928A;
