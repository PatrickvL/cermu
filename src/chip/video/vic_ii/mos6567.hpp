#pragma once

#include "chip/video/vic_ii/vicii_common.hpp"

inline constexpr VicIITraits MOS6567R8_traits = {
    .total_lines = 263, .visible_lines = 235, .cycles_per_line = 65,
    .visible_pixels_per_line = 418,
    .first_vblank_line = 13, .last_vblank_line = 40,
    .first_x_coord = 412, .first_visible_x_coord = 489, .last_visible_x_coord = 396,
    .hsync_start = 416, .hsync_end = 452, .burst_start = 456, .burst_end = 492,
    .framebuffer_start_x = 0, .framebuffer_end_x = 520,
    .chip_name = "MOS6567(R8) NTSC", .chip_id = "MOS6567", .vendor = "MOS Technology",
    .is_pal = false
};

inline constexpr VicIITraits MOS6567R56A_traits = {
    .total_lines = 262, .visible_lines = 234, .cycles_per_line = 64,
    .visible_pixels_per_line = 411,
    .first_vblank_line = 13, .last_vblank_line = 40,
    .first_x_coord = 412, .first_visible_x_coord = 488, .last_visible_x_coord = 388,
    .hsync_start = 416, .hsync_end = 452, .burst_start = 456, .burst_end = 492,
    .framebuffer_start_x = 0, .framebuffer_end_x = 520,
    .chip_name = "MOS6567(R56A) NTSC", .chip_id = "MOS6567", .vendor = "MOS Technology",
    .is_pal = false
};

// MOS6567 NTSC VIC-II — NTTP instantiation with NTSC R8 traits
using mos6567_t = vicii_t<MOS6567R8_traits>;
