#pragma once
/*
 * vic_traits.hpp — Compile-time traits for the MOS 6560/6561 VIC family
 *
 * Covers the full VIC-I family:
 *   MOS6560:  NTSC VIC (VIC-20 NTSC) — 65 cycles/line, 262 lines
 *   MOS6561:  PAL VIC  (VIC-20 PAL)  — 71 cycles/line, 312 lines
 *
 * Pattern follows fam65xx CPUTraits / VicIITraits — NTTP via const&,
 * inline constexpr instances in per-variant headers (mos6560.hpp,
 * mos6561.hpp).
 *
 * NOTE on PAL cycles/line:
 *   The MOS 6561 PAL variant actually generates 71 cycles per raster
 *   line, not 63 as the old code used.  VICE uses 71 and this matches
 *   the hardware crystal:  4.433619 MHz / 4 = 1,108,405 Hz system
 *   clock, divided by 71 cycles × 312 lines ≈ 50.05 Hz.  The old
 *   value of 63 was carried forward from an early approximation and
 *   should be corrected when the rendering pipeline is reviewed.
 *   For now, the traits preserve the existing working values to avoid
 *   regressions.
 */

#include <cstdint>

// ============================================================================
// VicTraits — compile-time descriptor for each VIC-I chip variant
// ============================================================================

struct VicTraits {
    // Identity
    const char* chip_name;          // Human-readable (e.g. "MOS6560 NTSC")
    const char* chip_id;            // Short chip ID (e.g. "MOS6560")
    const char* vendor;             // Manufacturer

    // Timing
    uint8_t  cycles_per_line;       // CPU cycles per raster line (NTSC: 65, PAL: 71)
    uint16_t total_lines;           // Total raster lines per frame (NTSC: 262, PAL: 312)
    uint32_t clock_frequency;       // Chip clock in Hz

    // Variant flag
    bool is_pal;

    // Helpers
    constexpr bool is_ntsc() const { return !is_pal; }
};

// Concrete trait instances live in their respective variant headers:
//   mos6560.hpp (NTSC), mos6561.hpp (PAL)
