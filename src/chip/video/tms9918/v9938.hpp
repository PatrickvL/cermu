#pragma once
/*
 * v9938.hpp — Yamaha V9938 VDP (MSX2)
 *
 * MSX2 Video Display Processor. Backward-compatible with TMS9918A plus:
 *   - 128 KB VRAM
 *   - 47 control registers, 9 status registers
 *   - Programmable 512-color palette (9-bit RGB, 16 entries)
 *   - Bitmap modes (Graphic 4–7: 256×212 at 4/8 bpp)
 *   - Hardware command engine (blitter)
 *   - Sprite mode 2 (16 colors per line, per-line attributes)
 *   - Vertical scroll register
 *   - Interlaced display modes
 *   - Mouse/trackball/light-pen input port
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits V9938Traits = {
    "Yamaha",                        // vendor
    "V9938",                         // chip_id
    VDPRegion::NTSC,                 // region
    VDPOutput::RGB,                  // output
    VDPSpriteModel::V9938,           // sprite_model
    VDPPaletteModel::PALETTE_512,    // palette_model
    VDPScrollModel::V9938,           // scroll_model
    VDPFeatureFlags::BITMAP_MODES
    | VDPFeatureFlags::VRAM_128K
    | VDPFeatureFlags::COMMAND_ENGINE
    | VDPFeatureFlags::INTERLACE
    | VDPFeatureFlags::MOUSE_PORT
    | VDPFeatureFlags::STATUS_EXT
    | VDPFeatureFlags::WAIT_STATE,   // feature_flags
    47,                              // num_registers
    128,                             // vram_size_kb
    262,                             // total_lines
    212,                             // visible_lines
    5'370'000,                       // dot_clock_hz
};

using V9938 = tms9918_t<V9938Traits>;

} // namespace tms9918

using V9938 = tms9918::V9938;
