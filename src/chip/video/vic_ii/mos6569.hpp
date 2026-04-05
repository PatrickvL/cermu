#pragma once

#include "chip/video/vic_ii/vicii_common.hpp"
#include "core/signal/sync_types.hpp"

inline constexpr VicIITraits MOS6569_traits = {
    .total_lines = 312, .visible_lines = 284, .cycles_per_line = 63,
    .visible_pixels_per_line = 403,
    .first_vblank_line = 300, .last_vblank_line = 15,
    .first_x_coord = 404, .first_visible_x_coord = 480, .last_visible_x_coord = 380,
    .hsync_start = 408, .hsync_end = 444, .burst_start = 448, .burst_end = 488,
    .framebuffer_start_x = 0, .framebuffer_end_x = 504,
    .chip_name = "MOS6569 PAL", .chip_id = "MOS6569", .vendor = "MOS Technology",
    .is_pal = true
};

// 8 pixels per CPU cycle — PAL is the worst-case VIC-II variant.
static_assert(MAX_SIGNAL_SAMPLES >= MOS6569_traits.cycles_per_line * 8 * MOS6569_traits.total_lines + SIGNAL_BUFFER_MARGIN,
              "MAX_SIGNAL_SAMPLES too small for VIC-II PAL (MOS 6569)");

// MOS6569 PAL VIC-II — NTTP instantiation with PAL traits
using mos6569_t = vicii_t<MOS6569_traits>;

