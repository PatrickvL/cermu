#pragma once

#include "chip/video/vic_ii/vicii_common.hpp"

// VIC-IIe variant (C128) — same timing as MOS6567, additional features
inline constexpr VicIITraits MOS8564_traits = {
    .total_lines = 263, .visible_lines = 235, .cycles_per_line = 65,
    .visible_pixels_per_line = 418,
    .first_vblank_line = 13, .last_vblank_line = 40,
    .first_x_coord = 412, .first_visible_x_coord = 489, .last_visible_x_coord = 396,
    .hsync_start = 416, .hsync_end = 452, .burst_start = 456, .burst_end = 492,
    .framebuffer_start_x = 0, .framebuffer_end_x = 520,
    .chip_name = "MOS8564 NTSC VIC-IIe", .chip_id = "MOS8564", .vendor = "MOS Technology",
    .is_pal = false
};

// MOS8564 NTSC VIC-IIe (C128) — NTTP instantiation with NTSC VIC-IIe traits
using mos8564_t = vicii_t<MOS8564_traits>;
