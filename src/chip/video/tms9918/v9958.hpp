#pragma once
/*
 * v9958.hpp — Yamaha V9958 VDP (MSX2+)
 *
 * MSX2+ Video Display Processor. Extends V9938 with:
 *   - YJK/YAE colour encoding (high-color mode)
 *   - Horizontal scroll register
 *   - Minor register additions (48+ control registers)
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits V9958Traits = {
    "Yamaha",                        // vendor
    "V9958",                         // chip_id
    VDPRegion::NTSC,                 // region
    VDPOutput::RGB,                  // output
    VDPSpriteModel::V9938,           // sprite_model
    VDPPaletteModel::PALETTE_YJK,    // palette_model
    VDPScrollModel::V9958,           // scroll_model
    VDPFeatureFlags::BITMAP_MODES
    | VDPFeatureFlags::VRAM_128K
    | VDPFeatureFlags::COMMAND_ENGINE
    | VDPFeatureFlags::INTERLACE
    | VDPFeatureFlags::MOUSE_PORT
    | VDPFeatureFlags::STATUS_EXT
    | VDPFeatureFlags::WAIT_STATE,   // feature_flags
    48,                              // num_registers
    128,                             // vram_size_kb
    262,                             // total_lines
    212,                             // visible_lines
    5'370'000,                       // dot_clock_hz
};

using V9958 = tms9918_t<V9958Traits>;

} // namespace tms9918

using V9958 = tms9918::V9958;
