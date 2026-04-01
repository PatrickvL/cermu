#pragma once
// um6845.hpp — UMC UM6845 / UM6845R CRTC (~1981)
//
// UMC second source.  UM6845 is CPC "type 1".
// UM6845R is the revised variant with different mid-frame write behaviour
// (CPC "type 3").

#include "chip/video/fam6845/crtc_common.hpp"

// UM6845 — CPC type 1
inline constexpr CRTCTraits UM6845_traits = {
    .chip_name          = "UM6845",
    .chip_id            = "UM6845",
    .vendor             = "UMC",
    .pin_count          = 40,
    .num_registers      = 18,
    .address_mask       = 0x1F,
    .readable_mask      = (1u << 14) | (1u << 15) | (1u << 16) | (1u << 17),
    .has_private_dram   = false,
    .has_block_copy     = false,
    .has_smooth_scroll  = false,
    .has_attribute_ram  = false,
    .has_rgbi_output    = false,
    .max_vram_size      = 0,
    .cpc_type           = 1,       // CPC type 1
    .cursor_regs_write_only = false,
    .r3_has_vsync_width     = true,
    .timing_latch_mode      = 0,
};

// UM6845R — CPC type 3 (different mid-frame timing)
inline constexpr CRTCTraits UM6845R_traits = {
    .chip_name          = "UM6845R",
    .chip_id            = "UM6845R",
    .vendor             = "UMC",
    .pin_count          = 40,
    .num_registers      = 18,
    .address_mask       = 0x1F,
    .readable_mask      = (1u << 14) | (1u << 15) | (1u << 16) | (1u << 17),
    .has_private_dram   = false,
    .has_block_copy     = false,
    .has_smooth_scroll  = false,
    .has_attribute_ram  = false,
    .has_rgbi_output    = false,
    .max_vram_size      = 0,
    .cpc_type           = 3,       // CPC type 3
    .cursor_regs_write_only = false,
    .r3_has_vsync_width     = true,
    .timing_latch_mode      = 1,   // Latches at HBLANK (differs from original)
};

using um6845_t  = crtc_t<UM6845_traits>;
using um6845r_t = crtc_t<UM6845R_traits>;
