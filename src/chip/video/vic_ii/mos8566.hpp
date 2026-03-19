#pragma once

#include "chip/video/vic_ii/vicii_common.hpp"

// VIC-IIe variant (C128) — same timing as MOS6569, additional features
inline constexpr VicIITraits MOS8566_traits = {
    .total_lines = 312, .visible_lines = 284, .cycles_per_line = 63,
    .visible_pixels_per_line = 403,
    .first_vblank_line = 300, .last_vblank_line = 15,
    .first_x_coord = 404, .first_visible_x_coord = 480, .last_visible_x_coord = 380,
    .hsync_start = 408, .hsync_end = 444, .burst_start = 448, .burst_end = 488,
    .framebuffer_start_x = 0, .framebuffer_end_x = 504,
    .chip_name = "MOS8566 PAL VIC-IIe", .chip_id = "MOS8566", .vendor = "MOS Technology",
    .is_pal = true
};

// MOS8566 PAL VIC-IIe (C128) — NTTP instantiation with PAL VIC-IIe traits
using mos8566_t = vicii_t<MOS8566_traits>;
