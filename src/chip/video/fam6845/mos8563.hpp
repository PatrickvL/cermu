#pragma once
// mos8563.hpp — MOS 8563 Video Display Controller (1984)
//
// Major superset of the MOS 6545 CRTC, used in the Commodore 128.
// Adds private 16KB DRAM subsystem, block-copy/fill DMA engine,
// smooth scroll (H+V), attribute RAM with RGBI colour, and direct
// RGBI video output.
//
// 48-pin DIP package.  I/O via two CPU-facing registers at $D600/$D601:
//   RS=0 read:  status register (bit 7=ready, 6=lpen, 5=vsync, 2:0=version)
//   RS=0 write: address register (selects R0-R36)
//   RS=1 read:  selected register data
//   RS=1 write: selected register data
//
// Used in: Commodore 128 (flat), Commodore 128D

#include "chip/video/fam6845/crtc_common.hpp"

inline constexpr CRTCTraits MOS8563_traits = {
    .chip_name          = "MOS 8563 VDC",
    .chip_id            = "MOS8563",
    .vendor             = "MOS Technology",
    .pin_count          = 48,
    .num_registers      = 37,
    .address_mask       = 0x3F,
    // VDC: all registers readable except R31 (data port has side effects)
    .readable_mask      = 0x1FFFFFFFF & ~(1u << 31),
    .has_private_dram   = true,
    .has_block_copy     = true,
    .has_smooth_scroll  = true,
    .has_attribute_ram  = true,
    .has_rgbi_output    = true,
    .max_vram_size      = 16384,     // 16 KB standard
    .cpc_type           = -1,
    .cursor_regs_write_only = false,
    .r3_has_vsync_width     = true,
    .timing_latch_mode      = 2,     // Timing changes latch at frame boundary
};

using mos8563_t = crtc_t<MOS8563_traits>;
