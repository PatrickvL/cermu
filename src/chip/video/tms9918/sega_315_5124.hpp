#pragma once
/*
 * sega_315_5124.hpp — Sega 315-5124 VDP (Master System NTSC)
 *
 * Sega Master System VDP. TMS9918A-compatible base with:
 *   - Mode 4 tile engine (4bpp tiles, Sega CRAM palette)
 *   - 11 control registers (R0–R10)
 *   - 64 sprites, 8 per scanline, 8×8 or 8×16
 *   - Per-line horizontal scroll + column vertical scroll
 *   - 32-entry Color RAM (6-bit RGB per entry)
 */

#include "chip/video/tms9918/tms9918.hpp"

namespace tms9918 {

inline constexpr VDPTraits SEGA_315_5124Traits = {
    "Sega",                          // vendor
    "315-5124",                      // chip_id
    VDPRegion::NTSC,                 // region
    VDPOutput::COMPOSITE,            // output
    VDPSpriteModel::SEGA,            // sprite_model
    VDPPaletteModel::FIXED_15,       // palette_model (base TMS modes use fixed)
    VDPScrollModel::SEGA,            // scroll_model
    VDPFeatureFlags::SEGA_MODE_EXT,  // feature_flags
    11,                              // num_registers
    16,                              // vram_size_kb
    262,                             // total_lines
    192,                             // visible_lines
    5'370'000,                       // dot_clock_hz
};

using SEGA_315_5124 = tms9918_t<SEGA_315_5124Traits>;

} // namespace tms9918

using SEGA_315_5124 = tms9918::SEGA_315_5124;
