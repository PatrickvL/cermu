#pragma once
/*
 * vicii_traits.hpp — Compile-time traits for the VIC-II family
 *
 * Covers all VIC-II variants:
 *   MOS6569:     PAL VIC-II (C64)
 *   MOS6567(R8): NTSC VIC-II (C64, late revision)
 *   MOS6567(R56A): NTSC VIC-II (C64, early revision)
 *   MOS8566:     PAL VIC-IIe (C128)
 *   MOS8564:     NTSC VIC-IIe (C128)
 *
 * Pattern follows fam65xx CPUTraits — NTTP via const&, inline constexpr
 * instances in per-variant headers (mos6569.hpp, mos6567.hpp, etc.).
 */

#include <cstdint>

// ============================================================================
// VicIITraits — compile-time descriptor for each chip variant
// ============================================================================

// VicIITraits — compile-time chip configuration for VIC-II NTTP variants.
// Follows the fam65xx_t<CPUTraits> pattern: all variant-specific constants
// are baked into the type, enabling zero-overhead dispatch and constexpr
// cache computation.
struct VicIITraits {
    // Timing parameters
    uint16_t total_lines;             // Total raster lines per frame (PAL: 312, NTSC: 262/263)
    uint16_t visible_lines;           // Visible raster lines (PAL: 284, NTSC: 234/235)
    uint8_t  cycles_per_line;         // CPU cycles per raster line (PAL: 63, NTSC: 64/65)
    uint16_t visible_pixels_per_line; // Visible pixels per line (PAL: 403, NTSC: 411/418)
    uint16_t first_vblank_line;       // First vblank line
    uint16_t last_vblank_line;        // Last vblank line
    uint16_t first_x_coord;           // X coordinate at cycle 0
    uint16_t first_visible_x_coord;   // First visible X coordinate
    uint16_t last_visible_x_coord;    // Last visible X coordinate

    // Framebuffer area bounds
    uint16_t framebuffer_start_x;
    uint16_t framebuffer_end_x;

    // Identity
    const char* chip_name;            // Human-readable chip name (e.g. "MOS6569 PAL")
    const char* chip_id;              // Short chip ID (e.g. "MOS6569")
    const char* vendor;               // Manufacturer (e.g. "MOS Technology")

    // Variant flags
    bool is_pal;                      // PAL vs NTSC (affects line-0 raster/IRQ timing)

    // Helpers
    constexpr bool is_ntsc() const { return !is_pal; }
};
