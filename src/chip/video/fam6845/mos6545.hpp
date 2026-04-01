#pragma once
// mos6545.hpp — MOS Technology 6545 CRTC (~1980)
//
// Commodore's derivative of the MC6845, used across the PET/CBM line:
// PET 2001 (later revisions), CBM 3000/4000/8000 series, CBM 500/700.
//
// Key differences from base MC6845:
//   - R14/R15 (cursor position) are write-only on some revisions
//   - Otherwise functionally identical timing model

#include "chip/video/fam6845/crtc_common.hpp"

inline constexpr CRTCTraits MOS6545_traits = {
    .chip_name          = "MOS 6545",
    .chip_id            = "MOS6545",
    .vendor             = "MOS Technology",
    .pin_count          = 40,
    .num_registers      = 18,
    .address_mask       = 0x1F,
    // R14-R15 write-only, R16-R17 readable (light pen)
    .readable_mask      = (1u << 16) | (1u << 17),
    .has_private_dram   = false,
    .has_block_copy     = false,
    .has_smooth_scroll  = false,
    .has_attribute_ram  = false,
    .has_rgbi_output    = false,
    .max_vram_size      = 0,
    .cpc_type           = -1,
    .cursor_regs_write_only = true,
    .r3_has_vsync_width     = true,
    .timing_latch_mode      = 0,
};

using mos6545_t = crtc_t<MOS6545_traits>;
