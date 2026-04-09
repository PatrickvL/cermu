#pragma once
/*
 * mc6809_vdg_constants.hpp — Constants for MC6809E + MC6847 VDG systems
 *
 * Shared between TRS-80 CoCo 1/2, Dragon 32/64, and any future
 * MC6809E + MC6847 computer variants.  All these machines use the same
 * I/O map, CPU clock, and display dimensions — only differing in
 * ROM content, keyboard layout, and PAL/NTSC timing.
 */

#include <cstdint>

namespace mc6809_vdg {

    // ── CPU ─────────────────────────────────────────────────────────────
    inline constexpr uint32_t CPU_FREQ_HZ              = 894886;    // 0.895 MHz (E-rate)
    inline constexpr uint32_t CPU_FREQ_FAST_HZ         = 1789772;   // 1.79 MHz (address rate)

    // ── Memory ──────────────────────────────────────────────────────────
    inline constexpr uint32_t BASIC_ROM_SIZE           = 8192;      // 8 KB
    inline constexpr uint32_t EXT_BASIC_ROM_SIZE       = 8192;      // 8 KB
    inline constexpr uint32_t MAX_CART_SIZE            = 16384;     // 16 KB

    // ── I/O addresses (identical between CoCo and Dragon) ───────────
    inline constexpr uint16_t PIA0_BASE                = 0xFF00;
    inline constexpr uint16_t PIA1_BASE                = 0xFF20;
    inline constexpr uint16_t SAM_BASE                 = 0xFFC0;

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int DISPLAY_WIDTH                 = 256;
    inline constexpr int DISPLAY_HEIGHT                = 192;

    // ── Timing ──────────────────────────────────────────────────────────
    // NTSC: 262 lines × 57.28 CPU cycles/line ≈ 14934 cycles/frame
    inline constexpr uint32_t CYCLES_PER_FRAME_NTSC    = 14934;
    // PAL: 312 lines × 57.28 CPU cycles/line ≈ 17872 cycles/frame
    inline constexpr uint32_t CYCLES_PER_FRAME_PAL     = 17872;

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE           = 44100;

} // namespace mc6809_vdg
