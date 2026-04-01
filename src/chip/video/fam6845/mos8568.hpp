#pragma once
// mos8568.hpp — MOS 8568 DVDC (Die-shrink/bug-fix VDC, ~1986)
//
// Used in the Commodore 128DCR (cost-reduced version).
// Functionally identical to the 8563 with minor bug fixes.
// Supports 64KB DRAM (vs 16KB on the 8563).

#include "chip/video/fam6845/crtc_common.hpp"

inline constexpr CRTCTraits MOS8568_traits = {
    .chip_name          = "MOS 8568 DVDC",
    .chip_id            = "MOS8568",
    .vendor             = "MOS Technology",
    .pin_count          = 48,
    .num_registers      = 37,
    .address_mask       = 0x3F,
    .readable_mask      = 0x1FFFFFFFF & ~(1u << 31),
    .has_private_dram   = true,
    .has_block_copy     = true,
    .has_smooth_scroll  = true,
    .has_attribute_ram  = true,
    .has_rgbi_output    = true,
    .max_vram_size      = 65536,     // 64 KB on C128DCR
    .cpc_type           = -1,
    .cursor_regs_write_only = false,
    .r3_has_vsync_width     = true,
    .timing_latch_mode      = 2,
};

using mos8568_t = crtc_t<MOS8568_traits>;
