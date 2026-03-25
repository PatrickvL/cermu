#pragma once
/*
 * ym_fm_traits.hpp — Compile-time traits for the Yamaha FM synthesizer family
 *
 * Covers the major product lines:
 *
 *   OPM family (4-op, no SSG):
 *     YM2151 (OPM):  8 FM channels, 4 operators, used in arcade / Sharp X68000
 *
 *   OPN family (4-op, with SSG):
 *     YM2203 (OPN):   3 FM + SSG, no ADPCM
 *     YM2608 (OPNA):  6 FM + SSG + ADPCM-A + ADPCM-B
 *     YM2610 (OPNB):  4 FM + SSG + ADPCM-A + ADPCM-B (Neo Geo)
 *     YM2612 (OPN2):  6 FM + DAC, no SSG (Mega Drive / Genesis)
 *     YM3438 (OPN2C): CMOS YM2612
 *
 *   OPL family (2-op):
 *     YM3526 (OPL):   9 FM channels, rhythm mode
 *     YM3812 (OPL2):  9 FM channels, waveform select
 *     YM2413 (OPLL):  9 FM channels, ROM patches, rhythm mode
 *
 * Design: YMTraits composes with AYTraits via pointer for chips that embed
 * an SSG (PSG) block. This mirrors the hardware — the SSG in OPN-family chips
 * is literally an AY-3-8910 equivalent circuit with its own register file and
 * clock domain.
 *
 * Pattern follows AYTraits / CPUTraits — NTTP via const&, inline constexpr
 * instances in per-variant headers.
 */

#include <cstdint>
#include "chip/sound/ay_psg/ay_psg_traits.hpp"

// ============================================================================
// YMTraits — compile-time descriptor for each FM chip variant
// ============================================================================

struct YMTraits {
    const char* vendor;
    const char* chip_id;
    const char* display_name;       // "Yamaha YM2612 (OPN2)" — human-readable UI label

    // FM core
    uint8_t fm_channels;            // 3, 4, 6, 8, or 9
    uint8_t operators_per_channel;  // 2 (OPL/OPLL) or 4 (OPN/OPM)
    uint8_t fm_algorithms;          // 4 (OPL) or 8 (OPN/OPM)

    // OPN ch3 special mode (per-operator frequency)
    bool has_ch3_special_mode;

    // LFO
    bool has_lfo;

    // Rhythm / percussion mode (OPLL, OPL, OPL2)
    bool has_rhythm_mode;

    // OPL2+ waveform select (sine variants)
    bool has_waveform_select;

    // OPLL ROM instrument patches
    bool has_rom_patches;

    // SSG — present = embedded AY equivalent
    bool             has_ssg;
    const AYTraits*  ssg;           // nullptr when has_ssg == false

    // ADPCM
    bool has_adpcm_a;   // YM2608, YM2610  (6-channel, low bitrate)
    bool has_adpcm_b;   // YM2608          (1-channel, high bitrate, streaming)

    // Direct DAC channel (YM2612 ch6 DAC mode)
    bool has_dac;

    // Package
    uint8_t pin_count;              // 18, 24, 40, or 64

    // === Helpers ===
    constexpr bool is_opl_family()    const { return operators_per_channel == 2; }
    constexpr bool is_opn_family()    const { return operators_per_channel == 4 && !is_opm(); }
    constexpr bool is_opm()           const { return fm_channels == 8 && !has_ssg && !has_rhythm_mode; }
    constexpr bool has_embedded_psg() const { return has_ssg && ssg != nullptr; }
};

// Concrete trait instances live in their respective variant headers:
//   ym2151.hpp, ym2203.hpp, ym2413.hpp, ym2608.hpp, ym2610.hpp,
//   ym2612.hpp, ym3438.hpp, ym3526.hpp, ym3812.hpp
