#pragma once
// mc6845_1.hpp — Motorola MC6845-1 CRTC (~1981)
//
// Speed-binned variant of the original MC6845.
// Used in some Amstrad CPC units (CPC "type 2").

#include "chip/video/fam6845/crtc_common.hpp"

inline constexpr CRTCTraits MC6845_1_traits = {
    .chip_name          = "MC6845-1",
    .chip_id            = "MC6845-1",
    .vendor             = "Motorola",
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
    .cpc_type           = 2,       // CPC type 2
    .cursor_regs_write_only = false,
    .r3_has_vsync_width     = true,
    .timing_latch_mode      = 0,
};

using mc6845_1_t = crtc_t<MC6845_1_traits>;
