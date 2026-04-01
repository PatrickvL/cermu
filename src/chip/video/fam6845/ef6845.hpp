#pragma once
// ef6845.hpp — SGS-Thomson EF6845 CRTC (~1979)
//
// Second source of the MC6845, used in Thomson TO7/MO5/TO9 computers.

#include "chip/video/fam6845/crtc_common.hpp"

inline constexpr CRTCTraits EF6845_traits = {
    .chip_name          = "EF6845",
    .chip_id            = "EF6845",
    .vendor             = "SGS-Thomson",
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

using ef6845_t = crtc_t<EF6845_traits>;
