#pragma once
/*
 * tms9918_traits.hpp — VDP Traits for TMS9918 family
 *
 * Non-type template parameter (NTTP) struct for the entire TMS9918 VDP family:
 *   TMS9918, TMS9918A, TMS9928A, TMS9929, TMS9929A,
 *   V9938, V9958, Sega 315-5124, Sega 315-5246.
 *
 * Follows the CPUTraits pattern from fam65xx_processor_traits.hpp.
 */

#include <cstdint>

namespace tms9918 {

// ============================================================================
// TRAIT AXIS ENUMS
// ============================================================================

enum class VDPRegion : uint8_t {
    NTSC,       // 262 lines, 3.579545 MHz colorburst
    PAL,        // 313 lines, 4.433619 MHz colorburst
};

enum class VDPOutput : uint8_t {
    COMPOSITE,  // TMS9918, TMS9918A, TMS9929A (internal composite DAC)
    RGB,        // TMS9928A (separate R/G/B/Y outputs, active logic)
};

enum class VDPSpriteModel : uint8_t {
    ORIGINAL,   // 4 sprites/line, 32 total, 8×8 or 16×16, zoom
    SEGA,       // SMS: 8 sprites/line, 64 total, 8×8 or 8×16
    V9938,      // V9938+: sprite mode 2 (16 colors/line, per-line color)
};

enum class VDPPaletteModel : uint8_t {
    FIXED_15,   // TMS9918/A/28/29 — 15 fixed colors + transparent
    PALETTE_512,// V9938 — 16 entries from 512-color 9-bit RGB palette
    PALETTE_YJK,// V9958 — adds YJK/YAE colour encoding (MSX2+)
};

enum class VDPScrollModel : uint8_t {
    NONE,       // TMS9918 base — no hardware scroll
    SEGA,       // SMS/GG — per-line horizontal + column vertical scroll
    V9938,      // V9938 — vertical scroll register only
    V9958,      // V9958 — vertical + horizontal scroll registers
};

// ============================================================================
// FEATURE FLAGS (bitmask)
// ============================================================================

namespace VDPFeatureFlags {
    inline constexpr uint32_t BITMAP_MODES     = 1u << 0;  // V9938+: Graphic 4-7
    inline constexpr uint32_t VRAM_128K        = 1u << 1;  // V9938+: 128KB VRAM
    inline constexpr uint32_t COMMAND_ENGINE   = 1u << 2;  // V9938+: hardware blitter
    inline constexpr uint32_t INTERLACE        = 1u << 3;  // V9938+: interlaced modes
    inline constexpr uint32_t MOUSE_PORT       = 1u << 4;  // V9938+: mouse/trackball input
    inline constexpr uint32_t SEGA_MODE_EXT    = 1u << 5;  // SMS: mode 4 tile engine
    inline constexpr uint32_t SEGA_GG_MODE     = 1u << 6;  // Game Gear: 12-bit CRAM, viewport
    inline constexpr uint32_t STATUS_EXT       = 1u << 7;  // V9938+: extended status registers
    inline constexpr uint32_t WAIT_STATE       = 1u << 8;  // V9938+: CPU wait signal
}

// ============================================================================
// VDP TRAITS STRUCT (NTTP)
// ============================================================================

struct VDPTraits {
    const char*      vendor;
    const char*      chip_id;
    const char*      display_name;    // "TI TMS9918A" — human-readable UI label
    VDPRegion        region;
    VDPOutput        output;
    VDPSpriteModel   sprite_model;
    VDPPaletteModel  palette_model;
    VDPScrollModel   scroll_model;
    uint32_t         feature_flags;
    uint8_t          num_registers;    // 8, 11 (Sega), 47 (V9938), 48+ (V9958)
    uint16_t         vram_size_kb;     // 16 or 128
    uint16_t         total_lines;      // 262 NTSC, 313 PAL
    uint16_t         visible_lines;    // 192 base, 212 V9938 extended
    uint32_t         dot_clock_hz;     // Dot clock frequency

    // === Convenience helpers (all constexpr) ===

    constexpr bool has(uint32_t flag) const {
        return (feature_flags & flag) != 0;
    }

    constexpr bool is_pal() const {
        return region == VDPRegion::PAL;
    }

    constexpr bool is_sega() const {
        return has(VDPFeatureFlags::SEGA_MODE_EXT);
    }

    constexpr bool is_v9938_class() const {
        return has(VDPFeatureFlags::BITMAP_MODES);
    }

    constexpr bool has_command_engine() const {
        return has(VDPFeatureFlags::COMMAND_ENGINE);
    }

    constexpr bool has_programmable_palette() const {
        return palette_model != VDPPaletteModel::FIXED_15;
    }

    constexpr bool has_extended_status() const {
        return has(VDPFeatureFlags::STATUS_EXT);
    }

    constexpr bool has_scroll() const {
        return scroll_model != VDPScrollModel::NONE;
    }

    constexpr uint32_t vram_mask() const {
        return (static_cast<uint32_t>(vram_size_kb) * 1024u) - 1u;
    }

    // Dots per scanline (342 for both NTSC and PAL TMS9918)
    static constexpr uint16_t dots_per_line = 342;
};

} // namespace tms9918
