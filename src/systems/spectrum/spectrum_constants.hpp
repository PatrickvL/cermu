#pragma once
/*
 * spectrum_constants.h — ZX Spectrum system constants
 *
 * Covers the ZX Spectrum 48K (1982) and ZX Spectrum 128K (1985).
 * Both are Z80A-based, differing primarily in memory, AY sound, and banking.
 */

#include <cstdint>

namespace spectrum_constants {

    // ── Timing (PAL only — the Spectrum was never NTSC) ─────────────────
    inline constexpr uint32_t CPU_FREQ_HZ         = 3500000;   // 3.5 MHz Z80A
    inline constexpr uint32_t TSTATES_PER_FRAME   = 224 * 312; // 69888 (224 T-states/line × 312 PAL lines)

    // ── Display ─────────────────────────────────────────────────────────
    inline constexpr int TOTAL_WIDTH              = 352;       // 48 + 256 + 48
    inline constexpr int TOTAL_HEIGHT             = 296;       // 48 + 192 + 56
    inline constexpr uint16_t SCREEN_BASE         = 0x4000;

    // ── Audio ───────────────────────────────────────────────────────────
    inline constexpr int DEFAULT_SAMPLE_RATE      = 44100;

} // namespace spectrum_constants
