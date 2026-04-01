#pragma once
// mc6845.hpp — Motorola MC6845 CRT Controller (1977)
//
// The root of the CRTC family.  The MC6845 generates timing and addressing
// for raster-scanned CRT displays.  It does NOT produce pixel data — it
// provides row/column addresses and sync signals.  The host system combines
// these with character ROM lookup and attribute RAM to produce the display.
//
// Used in: Commodore PET/CBM, BBC Micro, Amstrad CPC, IBM CGA/MDA,
//          many S-100 boards.
//
// 40-pin DIP, 18 registers, R3[7:4] VSYNC width fixed at 16 lines.

#include "chip/video/fam6845/crtc_common.hpp"

inline constexpr CRTCTraits MC6845_traits = {
    .chip_name          = "MC6845",
    .chip_id            = "MC6845",
    .vendor             = "Motorola",
    .pin_count          = 40,
    .num_registers      = 18,
    .address_mask       = 0x1F,
    // R14-R17 readable (cursor + light pen)
    .readable_mask      = (1u << 14) | (1u << 15) | (1u << 16) | (1u << 17),
    .has_private_dram   = false,
    .has_block_copy     = false,
    .has_smooth_scroll  = false,
    .has_attribute_ram  = false,
    .has_rgbi_output    = false,
    .max_vram_size      = 0,
    .cpc_type           = -1,
    .cursor_regs_write_only = false,
    .r3_has_vsync_width     = false,  // Original MC6845: VSYNC fixed at 16 lines
    .timing_latch_mode      = 0,      // Immediate
};

using mc6845_t = crtc_t<MC6845_traits>;
