#pragma once
/*
 * crtc_traits.hpp — Compile-time traits for the MC6845 CRTC family
 *
 * Covers the full lineage from the 1977 Motorola MC6845 through the
 * MOS 8563/8568 VDC used in the Commodore 128:
 *
 *   MC6845 (Motorola)          — the root
 *   R6545 (Rockwell)           — second source, minor R/W differences
 *   HD6845 / HD6845S (Hitachi) — second source (CPC type 0)
 *   EF6845 (SGS-Thomson)       — second source (Thomson TO7/MO5/TO9)
 *   UM6845 (UMC)               — second source (CPC type 1)
 *   UM6845R (UMC)              — revised mid-frame timing (CPC type 3)
 *   MC6845-1 (Motorola)        — speed-binned variant (CPC type 2)
 *   MOS 6545 (Commodore)       — Commodore's derivative (PET/CBM)
 *   MOS 8563 (Commodore)       — major superset: private DRAM, DMA, etc.
 *   MOS 8568 (Commodore)       — die-shrink / bug-fix of 8563 (C128DCR)
 *
 * Pattern follows fam65xx CPUTraits — NTTP via const&, inline constexpr
 * instances in per-variant headers.
 */

#include <cstdint>
#include "core/signal/sync_types.hpp"

// ============================================================================
// CRTCTraits — compile-time chip configuration for NTTP variants
// ============================================================================

struct CRTCTraits {
    // ── Identity ──────────────────────────────────────────────────────
    const char* chip_name;          // "MC6845", "MOS 8563 VDC" etc.
    const char* chip_id;            // Short: "MC6845", "MOS8563"
    const char* vendor;             // "Motorola", "MOS Technology" etc.

    // ── Package ───────────────────────────────────────────────────────
    uint8_t pin_count;              // 40 for base 6845, 48 for 8563/8568

    // ── Register model ────────────────────────────────────────────────
    uint8_t num_registers;          // 18 for 6845 variants, 37 for 8563/8568
    uint8_t address_mask;           // Mask for address register (0x1F for 6845, 0x3F for 8563)

    // Bitmask: which of R0-R17 are readable.
    // Most 6845 variants: only R14-R17 (cursor + light pen).
    // The R6545 variant: R14-R15 are write-only.
    uint32_t readable_mask;

    // ── VDC extensions (8563/8568 only) ───────────────────────────────
    // These flags gate large code paths via if constexpr.
    bool has_private_dram;          // Private DRAM subsystem (MMIO data port)
    bool has_block_copy;            // Hardware block-copy / fill DMA engine
    bool has_smooth_scroll;         // Hardware smooth scroll (H + V)
    bool has_attribute_ram;         // Colour attribute RAM in private DRAM
    bool has_rgbi_output;          // Drives RGBI video output signal

    // DRAM sizing
    uint32_t max_vram_size;         // 0 for 6845, 16384 for 8563, 65536 for 8568

    // ── Variant-specific timing quirks ────────────────────────────────
    // CPC "type" classification (Amstrad demo scene numbering).
    // -1 = not a CPC variant.  0=HD6845S, 1=UM6845, 2=MC6845-1, 3=UM6845R.
    int8_t cpc_type;

    // MOS 6545: cursor position registers (R14/R15) are write-only.
    bool cursor_regs_write_only;

    // R3 high nibble (VSYNC width) handling.
    // true  = R3[7:4] sets VSYNC width (most variants)
    // false = R3[7:4] fixed at 16 lines (original MC6845)
    bool r3_has_vsync_width;

    // Mid-frame register write latching behavior.
    // 0 = changes take effect immediately on next char clock
    // 1 = changes latch at HBLANK
    // 2 = changes latch at frame boundary
    // Affects R0–R9 timing registers specifically.
    uint8_t timing_latch_mode;

    // ── Helpers ───────────────────────────────────────────────────────
    constexpr bool is_vdc() const { return has_private_dram; }
    constexpr bool is_cpc_variant() const { return cpc_type >= 0; }
};

// ============================================================================
// VDC RGBI Palette — standard CGA/RGBI 16-colour palette
// ============================================================================
// The VDC attribute byte's colour nibble selects from this palette.
// The palette is CGA-compatible (the 1084 monitor uses the same RGBI encoding).

namespace fam6845 {

// Standard RGBI palette (ABGR format for direct framebuffer use).
// Index = IRGB: bit 3=Intensity, bit 2=Red, bit 1=Green, bit 0=Blue.
inline constexpr uint32_t RGBI_PALETTE[16] = {
    0xFF000000,  //  0: Black
    0xFFAA0000,  //  1: Blue
    0xFF00AA00,  //  2: Green
    0xFFAAAA00,  //  3: Cyan
    0xFF0000AA,  //  4: Red
    0xFFAA00AA,  //  5: Magenta
    0xFF0055AA,  //  6: Brown
    0xFFAAAAAA,  //  7: Light gray
    0xFF555555,  //  8: Dark gray
    0xFFFF5555,  //  9: Light blue
    0xFF55FF55,  // 10: Light green
    0xFFFFFF55,  // 11: Light cyan
    0xFF5555FF,  // 12: Light red
    0xFFFF55FF,  // 13: Light magenta
    0xFF55FFFF,  // 14: Yellow
    0xFFFFFFFF,  // 15: White
};

// ── Clock constants ──────────────────────────────────────────────────

inline constexpr uint32_t VDC_CRYSTAL_HZ  = 16000000;   // 16.000 MHz master clock
inline constexpr uint8_t  VDC_PIXELS_PER_CHAR = 8;      // Default (double-pixel = 16)
inline constexpr uint8_t  VDC_CHAR_CLOCK_DIV  = 16;     // CLK / 16 = 1 MHz char clock
// Worst-case frame dimensions (register-programmable; values from KERNAL
// initialization and PAL timing).  Used for signal buffer sizing.
inline constexpr uint32_t VDC_MAX_CHARS_PER_LINE = 128;  // R0+1 (KERNAL: R0=126 → 127)
inline constexpr uint32_t VDC_MAX_PPC_DOUBLE     = 16;   // Double-pixel mode
inline constexpr uint32_t VDC_MAX_LINES          = 313;  // PAL total raster lines
static_assert(MAX_SIGNAL_SAMPLES >= VDC_MAX_CHARS_PER_LINE * VDC_MAX_PPC_DOUBLE * VDC_MAX_LINES + SIGNAL_BUFFER_MARGIN,
              "MAX_SIGNAL_SAMPLES too small for VDC double-pixel mode");
// ── Status register bits ─────────────────────────────────────────────

inline constexpr uint8_t STATUS_READY       = 0x80;   // Bit 7: VDC ready for CPU access
inline constexpr uint8_t STATUS_LPEN        = 0x40;   // Bit 6: Light pen triggered
inline constexpr uint8_t STATUS_VSYNC       = 0x20;   // Bit 5: In vertical blanking
inline constexpr uint8_t STATUS_VERSION     = 0x07;   // Bits 2-0: Chip version

// VDC chip version codes (returned in status register bits 2-0)
inline constexpr uint8_t VDC_VERSION_8563   = 0x00;   // MOS 8563 (original)
inline constexpr uint8_t VDC_VERSION_8568   = 0x01;   // MOS 8568 (C128DCR)

} // namespace fam6845
