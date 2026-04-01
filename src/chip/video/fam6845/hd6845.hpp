#pragma once
// hd6845.hpp — Hitachi HD6845 / HD6845S CRTC (~1979)
//
// Second source of the MC6845.  The HD6845S is used in the BBC Micro
// and many Amstrad CPC units (CPC "type 0").

#include "chip/video/fam6845/crtc_common.hpp"

// HD6845S — the more common variant (BBC Micro, CPC type 0)
inline constexpr CRTCTraits HD6845S_traits = {
    .chip_name          = "HD6845S",
    .chip_id            = "HD6845S",
    .vendor             = "Hitachi",
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
    .cpc_type           = 0,       // CPC type 0
    .cursor_regs_write_only = false,
    .r3_has_vsync_width     = true,
    .timing_latch_mode      = 0,
};

// HD6845 — original Hitachi variant (less common)
inline constexpr CRTCTraits HD6845_traits = {
    .chip_name          = "HD6845",
    .chip_id            = "HD6845",
    .vendor             = "Hitachi",
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
    .cpc_type           = -1,
    .cursor_regs_write_only = false,
    .r3_has_vsync_width     = true,
    .timing_latch_mode      = 0,
};

using hd6845s_t = crtc_t<HD6845S_traits>;
using hd6845_t  = crtc_t<HD6845_traits>;
