#pragma once
// r6545.hpp — Rockwell R6545 CRTC (1978)
//
// Second source of the MC6845 with minor read/write differences.
// Some registers that are read/write on the 6845 are write-only here.

#include "chip/video/fam6845/crtc_common.hpp"

inline constexpr CRTCTraits R6545_traits = {
    .chip_name          = "R6545",
    .chip_id            = "R6545",
    .vendor             = "Rockwell",
    .pin_count          = 40,
    .num_registers      = 18,
    .address_mask       = 0x1F,
    // Only R16-R17 (light pen) readable; R14-R15 also readable
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

using r6545_t = crtc_t<R6545_traits>;
