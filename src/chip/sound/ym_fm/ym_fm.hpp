#pragma once
/*
 * ym_fm.hpp — Yamaha FM synthesizer family — template core
 *
 * NTTP-parameterized implementation covering the full Yamaha FM family.
 * Each variant is selected at compile time via YMTraits, enabling
 * zero-overhead feature dispatch with if constexpr.
 *
 * Architecture:
 *   - FM core: phase generator + envelope generator per operator,
 *     4-op (OPN/OPM) or 2-op (OPL) algorithm routing.
 *   - SSG: for chips with an embedded PSG, the SSG block is a
 *     separately-clocked AY circuit composed into this class.
 *   - ADPCM: stub blocks for ADPCM-A (rhythm) and ADPCM-B (streaming).
 *   - DAC: direct 8-bit DAC mode on channel 6 (YM2612).
 *
 * SHORTCOMINGS — known gaps vs. real hardware (in rough priority order):
 *
 *   1. Algorithm routing does not implement inter-operator FM modulation.
 *      Operators are advanced independently; compute_algorithm() merely
 *      selects which operator *outputs* to sum.  Real hardware feeds a
 *      modulator's output into the next operator's phase input, which is
 *      the entire basis of FM synthesis.  This must be rewritten so that
 *      operator evaluation order follows the algorithm graph, feeding each
 *      modulator result into the carrier's phase accumulator step.
 *
 *   2. Envelope generator is simplified.  Real hardware uses per-rate
 *      increment tables indexed by rate + key-scale + rof counter, with
 *      non-linear attack curves.  This implementation uses a crude linear
 *      approximation that will produce noticeably wrong volume contours.
 *
 *   3. SSG composition is declared but not wired.  YMTraits::ssg points
 *      to an AYTraits instance, but no ay_psg_t is instantiated or
 *      clocked inside ym_fm_t.  SSG output is therefore silent.
 *
 *   4. [DONE] Ch3 special mode — per-operator independent frequencies from
 *      supplementary F-Num registers ($A8-$AE) are now decoded and applied
 *      when Ch3 mode bits are set in register $27.
 *
 *   5. [DONE] Sine table now uses the hardware log-sin + exp ROM pipeline
 *      with integer arithmetic, replicating the characteristic quantization
 *      artifacts of real Yamaha FM chips.
 *
 *   6. [DONE] DT1 detune now uses the hardware-accurate 32-entry lookup
 *      table keyed by (block, keycode) for each of the 4 detune magnitudes.
 *      DT2 (OPM only) is not yet implemented.
 *
 *   7. ADPCM-A and ADPCM-B are unimplemented stubs — trait flags exist
 *      but no decode/playback logic is present.
 *
 *   8. OPL-family specifics partially implemented: waveform select (WS)
 *      lookup is wired per-operator (0=sine, 1=half, 2=abs, 3=quarter),
 *      but OPL register decode is missing — the waveform field is never
 *      written.  Rhythm mode percussion and OPLL ROM patches are absent.
 *      Requires an OPL register-decode path (separate from OPN) to become
 *      functional.
 *
 *   9. OPM-specific features missing: noise channel, key-fraction
 *      register, and the OPM-specific channel/operator addressing.
 *
 *  10. [DONE] Rate-scaling is now applied — RS bits and keycode scale the
 *      effective envelope rate via (2*rate + keycode>>(3-rs)), clamped to 63.
 *
 *  11. [DONE] SSG-EG control implemented — enable/attack/alternate/hold
 *      bits now alter the envelope shape (inversion + restart/hold on
 *      reaching max attenuation).
 *
 *  12. [DONE] LFO AM/PM modulation is now applied to operators — PM
 *      modulates the phase increment proportionally via PMS sensitivity,
 *      AM adds attenuation to the envelope level via AMS sensitivity.
 *
 *  13. [DONE] YM2612 ladder-effect DAC distortion modeled via traits flag.
 *      The NMOS DAC's zero-crossing offset is applied when ladder_effect=true.
 *
 *  14. Timer prescaling differs between OPN sub-variants; this
 *      implementation uses a single advance-per-tick model.
 *
 *  15. Bus protocol is minimal — tick() does not decode address/data
 *      from bus_state_t; callers must use latch_address()/write_register()
 *      directly.  A proper bus decode should be added.
 *
 * Bus interface:
 *   tick(bus_state_t) receives and returns the bus word each clock.
 *   Register access is via the A0(/A1) address latch + data write protocol
 *   common to all Yamaha FM chips.
 *
 * Audio output:
 *   Driven exclusively through AudioPort (accumulator-decimation model).
 *   No internal ring buffer — the port handles sample-rate conversion.
 */

#include "chip/sound/ym_fm/ym_fm_traits.hpp"
#include "chip/sound/sound_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include "core/signal/audio_port.hpp"
#include "core/system_lines.hpp"
#include <cstdint>
#include <cstring>
#ifndef _USE_MATH_DEFINES
  #define _USE_MATH_DEFINES   // M_PI on MSVC
#endif
#include <cmath>

// ============================================================================
// YM FM REGISTER DECLARATION TABLE — single source of truth
// ============================================================================
//
// This table covers the OPN-family common register map (bank 0, $20-$B6).
// OPL and OPM share the same structural pattern but with different offsets;
// the DECL table captures the OPN superset and the implementation uses
// if constexpr to gate variant-specific registers.
//
// The per-operator registers ($30-$9F) repeat for each of 3 channels ×
// 4 operators within each bank.  The DECL table lists them once; the
// implementation indexes by channel + operator slot.
//
// Register map reference:
//   $21 — Test / LSI test data
//   $22 — LFO frequency (OPN2/OPNA)
//   $24 — Timer A MSB
//   $25 — Timer A LSB (low 2 bits)
//   $26 — Timer B
//   $27 — Ch3 mode / Timer control
//   $28 — Key on/off
//   $2A — DAC data (OPN2)
//   $2B — DAC enable (OPN2)
//   $30-$3E — DT1/MUL (per-operator, per-channel)
//   $40-$4E — TL (total level, per-operator, per-channel)
//   $50-$5E — RS/AR (rate scaling / attack rate)
//   $60-$6E — AM/D1R (AM enable / first decay rate)
//   $70-$7E — D2R (second decay rate, sustain rate)
//   $80-$8E — D1L/RR (sustain level / release rate)
//   $90-$9E — SSG-EG (SSG-type envelope, OPN only)
//   $A0-$A2 — F-Num LSB (per-channel)
//   $A4-$A6 — Block/F-Num MSB (per-channel)
//   $A8-$AA — Ch3 supplementary F-Num (ch3 special mode)
//   $AC-$AE — Ch3 supplementary Block/F-Num MSB
//   $B0-$B2 — FB/Algorithm (per-channel)
//   $B4-$B6 — L/R/AMS/PMS (per-channel, stereo + LFO sensitivity)

#define YM_FM_DECL(REG, FLD, CMP) \
    REG(0x00, TEST,      "Test / LSI test data")                                 \
    REG(0x01, LFO_FREQ,  "LFO frequency")                                       \
      FLD(LFO_FREQ, LFO_EN,   3:3, "LFO enable",             Flag,  0, 0)       \
      FLD(LFO_FREQ, LFO_RATE, 2:0, "LFO frequency select",   Value, 0, 0)       \
    REG(0x02, TIMER_A_H, "Timer A high 8 bits")                                  \
    REG(0x03, TIMER_A_L, "Timer A low 2 bits")                                   \
      FLD(TIMER_A_L, TA_LOW, 1:0, "Timer A low bits",         Value, 0, 0)       \
    REG(0x04, TIMER_B,   "Timer B")                                              \
    REG(0x05, CH3_TIMER, "Ch3 mode / Timer control")                             \
      FLD(CH3_TIMER, CH3_MODE, 7:6, "Ch3 special mode",       Value, 0, 0)       \
      FLD(CH3_TIMER, RST_B,   5:5, "Timer B reset",           Flag,  0, 0)       \
      FLD(CH3_TIMER, RST_A,   4:4, "Timer A reset",           Flag,  0, 0)       \
      FLD(CH3_TIMER, EN_B,    3:3, "Timer B enable",           Flag,  0, 0)       \
      FLD(CH3_TIMER, EN_A,    2:2, "Timer A enable",           Flag,  0, 0)       \
      FLD(CH3_TIMER, LOAD_B,  1:1, "Timer B load",            Flag,  0, 0)       \
      FLD(CH3_TIMER, LOAD_A,  0:0, "Timer A load",            Flag,  0, 0)       \
    REG(0x06, KEY_ONOFF, "Key on/off")                                           \
      FLD(KEY_ONOFF, OP_MASK, 7:4, "Operator on mask",        Value, 0, 0)       \
      FLD(KEY_ONOFF, CH_SEL,  2:0, "Channel select",          Value, 0, 0)       \
    REG(0x07, DAC_DATA,  "DAC data (OPN2)")                                      \
    REG(0x08, DAC_EN,    "DAC enable (OPN2)")                                    \
      FLD(DAC_EN, DAC_ENABLE, 7:7, "DAC mode",                Flag,  0, 0)       \
    REG(0x09, DT1_MUL,   "DT1 / MUL (per-op)")                                  \
      FLD(DT1_MUL, DT1,   6:4, "Detune 1",                   Value, 0, 0)       \
      FLD(DT1_MUL, MUL,   3:0, "Frequency multiply",         Value, 0, 0)       \
    REG(0x0A, TL,        "Total level (per-op)")                                 \
      FLD(TL, TOTAL_LVL, 6:0, "Total level (attenuation)",    Level, 0, 0)       \
    REG(0x0B, RS_AR,     "Rate scaling / Attack rate (per-op)")                  \
      FLD(RS_AR, RS,      7:6, "Rate scaling",                Value, 0, 0)       \
      FLD(RS_AR, AR,      4:0, "Attack rate",                 Value, 0, 0)       \
    REG(0x0C, AM_D1R,    "AM enable / Decay 1 rate (per-op)")                    \
      FLD(AM_D1R, AM_EN,  7:7, "Amplitude modulation",        Flag,  0, 0)       \
      FLD(AM_D1R, D1R,    4:0, "First decay rate",            Value, 0, 0)       \
    REG(0x0D, D2R,       "Decay 2 rate / Sustain rate (per-op)")                 \
      FLD(D2R, D2_RATE,   4:0, "Second decay rate",           Value, 0, 0)       \
    REG(0x0E, D1L_RR,    "Sustain level / Release rate (per-op)")                \
      FLD(D1L_RR, D1L,    7:4, "Sustain level",               Level, 0, 0)       \
      FLD(D1L_RR, RR,     3:0, "Release rate",                Value, 0, 0)       \
    REG(0x0F, SSG_EG,    "SSG-type envelope (per-op)")                           \
      FLD(SSG_EG, SSG_EN, 3:3, "SSG-EG enable",               Flag,  0, 0)       \
      FLD(SSG_EG, SSG_SHAPE, 2:0, "SSG-EG shape",             Value, 0, 0)       \
    REG(0x10, FNUM_L,    "F-Number low 8 bits (per-ch)")                         \
    REG(0x11, BLOCK_FNUM,"Block / F-Number high (per-ch)")                       \
      FLD(BLOCK_FNUM, BLOCK, 5:3, "Block (octave)",           Value, 0, 0)       \
      FLD(BLOCK_FNUM, FNUM_H, 2:0, "F-Number high bits",      Value, 0, 0)       \
    REG(0x12, FB_ALG,    "Feedback / Algorithm (per-ch)")                        \
      FLD(FB_ALG, FB,     5:3, "Feedback level",              Value, 0, 0)       \
      FLD(FB_ALG, ALG,    2:0, "Algorithm",                   Value, 0, 0)       \
    REG(0x13, LR_AMS_PMS,"L/R output / AMS / PMS (per-ch)")                     \
      FLD(LR_AMS_PMS, L,     7:7, "Left output",             Flag,  0, 0)       \
      FLD(LR_AMS_PMS, R,     6:6, "Right output",            Flag,  0, 0)       \
      FLD(LR_AMS_PMS, AMS,   5:4, "AM sensitivity",          Value, 0, 0)       \
      FLD(LR_AMS_PMS, PMS,   2:0, "PM sensitivity",          Value, 0, 0)

// ============================================================================
// Extract constants and debug metadata
// ============================================================================

namespace ym_fm {
namespace reg {
    YM_FM_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr uint8_t DECL_REG_COUNT = 20;  // Distinct DECL entries

    // Actual hardware register file size (both banks)
    constexpr uint16_t OPN_BANK_SIZE   = 0x100;
    constexpr uint16_t OPN_TOTAL_REGS  = 0x100;  // Single bank for non-banked chips
    constexpr uint16_t OPN2_TOTAL_REGS = 0x100;  // Each bank is 256; we store one flat file

    // Key on/off register (not per-bank — always $28 in bank 0)
    constexpr uint8_t KEY_ONOFF_ADDR = 0x28;

    // Per-operator register base addresses (hardware addresses)
    constexpr uint8_t OP_DT1_MUL_BASE = 0x30;
    constexpr uint8_t OP_TL_BASE      = 0x40;
    constexpr uint8_t OP_RS_AR_BASE   = 0x50;
    constexpr uint8_t OP_AM_D1R_BASE  = 0x60;
    constexpr uint8_t OP_D2R_BASE     = 0x70;
    constexpr uint8_t OP_D1L_RR_BASE  = 0x80;
    constexpr uint8_t OP_SSG_EG_BASE  = 0x90;

    // Per-channel register base addresses (hardware addresses)
    constexpr uint8_t CH_FNUM_L_BASE      = 0xA0;
    constexpr uint8_t CH_BLOCK_FNUM_BASE  = 0xA4;
    constexpr uint8_t CH_FB_ALG_BASE      = 0xB0;
    constexpr uint8_t CH_LR_AMS_PMS_BASE  = 0xB4;

    // Timer / global registers
    constexpr uint8_t LFO_REG           = 0x22;
    constexpr uint8_t TIMER_A_H_REG     = 0x24;
    constexpr uint8_t TIMER_A_L_REG     = 0x25;
    constexpr uint8_t TIMER_B_REG       = 0x26;
    constexpr uint8_t CH3_TIMER_REG     = 0x27;
    constexpr uint8_t DAC_DATA_REG      = 0x2A;
    constexpr uint8_t DAC_EN_REG        = 0x2B;

    // Ch3 supplementary frequency (special mode)
    constexpr uint8_t CH3_FNUM_BASE     = 0xA8;
    constexpr uint8_t CH3_BLOCK_FNUM_BASE = 0xAC;
} // namespace reg

namespace fld {
#define YM_FM_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
YM_FM_DECL(DECL_REG_NOP, YM_FM_X_FLD_NS_, DECL_CMP_NOP)
#undef YM_FM_X_FLD_NS_
} // namespace fld
} // namespace ym_fm

DECL_EXTRACT(YM_FM, YM_FM_DECL)

// ============================================================================
// FM CONSTANTS
// ============================================================================

namespace ym_fm_constants {
    inline constexpr int MAX_FM_CHANNELS   = 9;   // OPL family maximum
    inline constexpr int MAX_OPS_PER_CH    = 4;   // OPN/OPM maximum
    inline constexpr int MAX_OPERATORS     = MAX_FM_CHANNELS * MAX_OPS_PER_CH;
    inline constexpr int PHASE_BITS        = 20;  // Phase accumulator precision
    inline constexpr int ENV_BITS          = 10;  // Envelope attenuation precision
    inline constexpr int SINE_TABLE_SIZE   = 1024;
    inline constexpr int ENV_MAX           = (1 << ENV_BITS) - 1;  // Full attenuation
    inline constexpr int TL_SHIFT          = 3;   // TL is in 0.75 dB steps → shift to env scale

    // DT1 detune magnitude table: indexed by [dt1_mag (0-3)][keycode (0-31)]
    // keycode = (block << 2) | (fnum >> 9)
    // dt1 bit 2 gives sign (0=pos, 1=neg); magnitude from dt1 bits 1:0
    inline constexpr uint8_t DT1_LUT[4][32] = {
        { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
        { 0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,
          2,  3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  8,  8,  8 },
        { 1,  1,  1,  1,  2,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,
          5,  6,  6,  7,  8,  8,  9, 10, 11, 12, 13, 14, 16, 16, 16, 16 },
        { 2,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,
          8,  8,  9, 10, 11, 12, 13, 14, 16, 17, 19, 20, 22, 22, 22, 22 }
    };
} // namespace ym_fm_constants

// ============================================================================
// FM OPERATOR STATE
// ============================================================================

struct FMOperator {
    // Phase generator
    uint32_t phase       = 0;       // Phase accumulator (PHASE_BITS)
    uint32_t freq        = 0;       // Frequency word (block + f-num derived)
    uint8_t  dt1         = 0;       // Detune 1
    uint8_t  mul         = 0;       // Frequency multiplier

    // Envelope generator
    uint16_t env_level   = ym_fm_constants::ENV_MAX;  // Current attenuation
    uint8_t  env_state   = 0;       // 0=off, 1=attack, 2=decay1, 3=decay2, 4=release
    uint8_t  tl          = 0;       // Total level (attenuation floor)
    uint8_t  ar          = 0;       // Attack rate
    uint8_t  d1r         = 0;       // First decay rate
    uint8_t  d2r         = 0;       // Second decay rate (sustain rate)
    uint8_t  rr          = 0;       // Release rate
    uint8_t  d1l         = 0;       // Sustain level (first decay target)
    uint8_t  rs          = 0;       // Rate scaling
    bool     am_en       = false;   // Amplitude modulation enable
    uint8_t  ssg_eg      = 0;       // SSG-EG control
    uint8_t  keycode     = 0;       // (block << 2) | (fnum >> 9) for DT1 + rate-scaling
    uint8_t  waveform    = 0;       // OPL2 waveform select (0-3)
    bool     ssg_inverted = false;  // SSG-EG output inversion state

    // Key state
    bool     key_on      = false;

    // Output
    int32_t  output      = 0;       // Last computed sample
    int32_t  prev_output = 0;       // Previous sample (for feedback)

    enum EnvState : uint8_t { OFF = 0, ATTACK, DECAY1, DECAY2, RELEASE };
};

// ============================================================================
// FM CHANNEL STATE
// ============================================================================

struct FMChannel {
    FMOperator ops[ym_fm_constants::MAX_OPS_PER_CH];

    uint16_t fnum       = 0;       // F-Number (frequency)
    uint8_t  block      = 0;       // Block (octave)
    uint8_t  feedback   = 0;       // Self-feedback level (0-7)
    uint8_t  algorithm  = 0;       // Algorithm select (0-7)
    bool     left       = true;    // Left output enable
    bool     right      = true;    // Right output enable
    uint8_t  ams        = 0;       // AM sensitivity
    uint8_t  pms        = 0;       // PM sensitivity
    int32_t  output     = 0;       // Mixed channel output
};

// ============================================================================
// SINE TABLE (log-sin → linear conversion)
// ============================================================================
//
// The YM2612 uses a log-sin ROM for phase→amplitude conversion.
// We precompute it as a signed 14-bit sine table.
//
// Hardware pipeline (replicated here with integer arithmetic):
//   1. 10-bit phase → quarter-wave mirror (8-bit index)
//   2. logsin ROM [256] → ~12-bit log attenuation
//   3. Split attenuation: integer part (shift) + fractional part (exp index)
//   4. exp ROM [256] + implicit bit 10 → 11-bit mantissa
//   5. mantissa >> shift → linear amplitude (quantized!)
//   6. Negate for quadrants 2 & 4
//
// The limited ROM resolution (256 entries each) introduces characteristic
// quantization steps — especially at low amplitudes where the right-shift
// loses precision.  This is a key part of the Yamaha FM sound.

namespace ym_fm_tables {

inline constexpr int SINE_TABLE_BITS = 10;
inline constexpr int SINE_TABLE_SIZE = 1 << SINE_TABLE_BITS;

// Runtime-initialized tables (populated via init_sine_table)
inline int16_t  sine_table[SINE_TABLE_SIZE];
inline uint16_t logsin_rom[256];            // quarter-wave log-sin ROM
inline uint16_t exp_rom[256];               // exponential ROM
inline bool     sine_table_initialized = false;

inline void init_sine_table() {
    if (sine_table_initialized) return;

    // Build quarter-wave log-sin ROM (256 entries, ~12-bit unsigned values)
    // logsin(i) = round(-log₂(sin((2i+1) × π / 2048)) × 256)
    for (int i = 0; i < 256; i++) {
        double angle = (2.0 * i + 1.0) / 1024.0 * (M_PI / 2.0);
        logsin_rom[i] = static_cast<uint16_t>(-std::log2(std::sin(angle)) * 256.0 + 0.5);
    }

    // Build exponential ROM (256 entries, 10-bit unsigned values)
    // exp(i) = round((2^((255-i)/256) - 1) × 1024)
    for (int i = 0; i < 256; i++) {
        double val = (std::pow(2.0, (255.0 - i) / 256.0) - 1.0) * 1024.0;
        exp_rom[i] = static_cast<uint16_t>(val + 0.5);
    }

    // Build full 1024-entry sine table using the integer pipeline
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        bool     negate = (i >> 9) & 1;     // bit 9: negative half-cycle
        bool     mirror = (i >> 8) & 1;     // bit 8: descending quarter
        uint8_t  idx    = i & 0xFF;
        if (mirror) idx = ~idx;              // 255 - idx

        uint16_t att   = logsin_rom[idx];    // ~12-bit log attenuation
        uint16_t frac  = att & 0xFF;         // lower 8 bits → exp ROM index
        uint16_t shift = att >> 8;           // upper bits   → right-shift amount

        // exp ROM + implicit bit 10 → 11-bit mantissa (1024..2047)
        uint16_t mantissa = exp_rom[frac] | 0x400;
        uint16_t linear   = mantissa >> shift;

        // Scale to 13-bit range (<<2) to match existing caller expectations.
        // Hardware peak ≈ 2045; ×4 ≈ 8180 (close to the ideal 8191, the
        // small deficit IS the hardware quantization).
        int16_t result = static_cast<int16_t>(linear << 2);
        if (negate) result = -result;
        sine_table[i] = result;
    }
    sine_table_initialized = true;
}

} // namespace ym_fm_tables

// ============================================================================
// ym_fm_t — Yamaha FM chip family template
// ============================================================================

template <const YMTraits& Traits>
class ym_fm_t : public SoundChipBase {
public:
    static constexpr uint8_t NUM_FM_CH  = Traits.fm_channels;
    static constexpr uint8_t NUM_OPS    = Traits.operators_per_channel;
    static constexpr uint8_t TOTAL_OPS  = NUM_FM_CH * NUM_OPS;

    // === Compile-time feature detection ===
    static constexpr bool is_opm()            { return Traits.is_opm(); }
    static constexpr bool is_opn_family()     { return Traits.is_opn_family(); }
    static constexpr bool is_opl_family()     { return Traits.is_opl_family(); }
    static constexpr bool has_embedded_psg()  { return Traits.has_embedded_psg(); }
    static constexpr bool has_ch3_special()   { return Traits.has_ch3_special_mode; }
    static constexpr bool has_lfo()           { return Traits.has_lfo; }
    static constexpr bool has_rhythm_mode()   { return Traits.has_rhythm_mode; }
    static constexpr bool has_waveform_sel()  { return Traits.has_waveform_select; }
    static constexpr bool has_rom_patches()   { return Traits.has_rom_patches; }
    static constexpr bool has_adpcm_a()       { return Traits.has_adpcm_a; }
    static constexpr bool has_adpcm_b()       { return Traits.has_adpcm_b; }
    static constexpr bool has_dac()           { return Traits.has_dac; }

    // ========================================================================
    // Construction
    // ========================================================================

    ym_fm_t()
        : SoundChipBase(ChipInfo{Traits.chip_id, Traits.vendor, Traits.display_name})
    {
        init_regs(ym_fm::reg::OPN_TOTAL_REGS);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
        ym_fm_tables::init_sine_table();
    }

    // ========================================================================
    // Initialization and reset
    // ========================================================================

    void init() {
        reset();
    }

    void reset() {
        regs_.clear();

        for (auto& ch : channel_) {
            ch.fnum      = 0;
            ch.block     = 0;
            ch.feedback  = 0;
            ch.algorithm = 0;
            ch.left      = true;
            ch.right     = true;
            ch.ams       = 0;
            ch.pms       = 0;
            ch.output    = 0;

            for (auto& op : ch.ops) {
                op.phase       = 0;
                op.freq        = 0;
                op.dt1         = 0;
                op.mul         = 0;
                op.env_level   = ym_fm_constants::ENV_MAX;
                op.env_state   = FMOperator::OFF;
                op.tl          = 0x7F;
                op.ar          = 0;
                op.d1r         = 0;
                op.d2r         = 0;
                op.rr          = 0;
                op.d1l         = 0;
                op.rs          = 0;
                op.am_en       = false;
                op.ssg_eg      = 0;
                op.keycode     = 0;
                op.waveform    = 0;
                op.ssg_inverted = false;
                op.key_on      = false;
                op.output      = 0;
                op.prev_output = 0;
            }
        }

        latch_addr_ = 0;
        latch_bank_ = 0;
        timer_a_ = 0;
        timer_b_ = 0;
        timer_a_counter_ = 0;
        timer_b_counter_ = 0;
        timer_a_overflow_ = false;
        timer_b_overflow_ = false;
        lfo_counter_ = 0;
        lfo_am_ = 0;
        lfo_pm_ = 0;
        for (int i = 0; i < 3; i++) { ch3_fnum_[i] = 0; ch3_block_[i] = 0; }
        dac_value_ = 0;
        dac_enabled_ = false;
        status_ = 0;
    }

    // ========================================================================
    // Audio output — AudioPort only
    // ========================================================================

    void set_audio_port(AudioPort* port) { audio_port_ = port; }

    // ========================================================================
    // Register interface — address latch + read/write
    // ========================================================================

    /// Latch the register address.  For OPN2, bit 1 of the control address
    /// selects bank 0 or bank 1 (A1 line).
    void latch_address(uint8_t addr, uint8_t bank = 0) {
        latch_addr_ = addr;
        latch_bank_ = bank;
    }

    /// Write data to the currently latched register address.
    void write_register(uint8_t data) {
        write_register(latch_addr_, data, latch_bank_);
    }

    /// Direct addressed write.
    void write_register(uint8_t addr, uint8_t data, uint8_t bank = 0) {
        // Store in register file
        regs_[addr] = data;

        // Global registers (bank 0 only, $20-$2F)
        if (bank == 0 && addr < 0x30) {
            on_global_write(addr, data);
            return;
        }

        // Per-operator registers ($30-$9F)
        if (addr >= 0x30 && addr < 0xA0) {
            on_operator_write(addr, data, bank);
            return;
        }

        // Per-channel registers ($A0-$BF)
        if (addr >= 0xA0 && addr < 0xC0) {
            on_channel_write(addr, data, bank);
            return;
        }
    }

    /// Read status register.
    uint8_t read_status() const {
        return status_;
    }

    // ========================================================================
    // Execution — call once per FM master clock cycle
    // ========================================================================
    //
    // The FM chips run at their master clock.  Internally the FM sample rate
    // is master_clock / (prescaler * 24) for OPN-family, or
    // master_clock / 64 for OPM.
    //
    // Each tick:
    //   1. Advance timers
    //   2. Advance LFO (if present)
    //   3. Advance FM operators (phase + envelope)
    //   4. Compute channel outputs via algorithm routing
    //   5. Mix and drive AudioPort
    //
    // SHORTCOMING: tick() does not decode address/data from bus_state_t.
    // Callers must use latch_address()/write_register() directly.  A full
    // bus protocol (active-low CS, WR, RD, A0/A1 decode) should be added.

    bus_state_t tick(bus_state_t pins) {
        // --- Timers ---
        advance_timers();

        // --- LFO ---
        if constexpr (has_lfo()) {
            advance_lfo();
        }

        // --- FM sample generation (at internal sample rate) ---
        // OPN: master / 144 (6 × 24), OPM: master / 64, OPL: master / 72
        if (++sample_divider_ >= sample_prescaler()) {
            sample_divider_ = 0;
            generate_fm_sample();
        }

        // --- Drive audio ---
        if (audio_port_) {
            audio_port_->drive(last_sample_);
        }

        // --- Update status on bus (active-low IRQ if timer overflow) ---
        if constexpr (has_ch3_special()) {
            if (timer_a_overflow_ || timer_b_overflow_) {
                BUS_CLR_BIT(pins, BUS_IRQ_BIT);
            }
        }

        bus_snapshot_ = pins;
        return pins;
    }

    /// Get current mixed mono sample (float, -1.0 to +1.0).
    float get_sample() const {
        return last_sample_;
    }

    // ========================================================================
    // State — public for debug inspection
    // ========================================================================

    FMChannel channel_[ym_fm_constants::MAX_FM_CHANNELS] = {};
    uint8_t   status_ = 0;

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif

private:
    AudioPort* audio_port_ = nullptr;
    float      last_sample_ = 0.f;

    // Address latch
    uint8_t latch_addr_ = 0;
    uint8_t latch_bank_ = 0;  // 0 or 1 (OPN2 dual-bank)

    // Timers
    uint16_t timer_a_ = 0;         // 10-bit Timer A period
    uint8_t  timer_b_ = 0;         // 8-bit Timer B period
    uint16_t timer_a_counter_ = 0;
    uint16_t timer_b_counter_ = 0;
    bool     timer_a_overflow_ = false;
    bool     timer_b_overflow_ = false;

    // LFO
    uint32_t lfo_counter_ = 0;
    uint8_t  lfo_am_ = 0;          // Current AM modulation value
    int8_t   lfo_pm_ = 0;          // Current PM modulation value

    // Ch3 special mode — per-operator frequencies (slots 0-2; slot 3 uses normal ch3 freq)
    uint16_t ch3_fnum_[3] = {};
    uint8_t  ch3_block_[3] = {};

    // DAC (OPN2)
    uint8_t  dac_value_ = 0;
    bool     dac_enabled_ = false;

    // Sample rate divider
    uint16_t sample_divider_ = 0;

    // ========================================================================
    // Sample rate prescaler — varies by family
    // ========================================================================

    static constexpr uint16_t sample_prescaler() {
        if constexpr (is_opm())        return 64;   // OPM: master / 64
        else if constexpr (is_opl_family()) return 72;   // OPL: master / 72
        else                           return 144;  // OPN: master / 144 (6 × 24)
    }

    // ========================================================================
    // Timer advancement
    // ========================================================================

    void advance_timers() {
        uint8_t ctrl = regs_[ym_fm::reg::CH3_TIMER_REG];

        // Timer A: counts up, overflows at 1024
        if (ctrl & ym_fm::fld::CH3_TIMER_EN_A) {
            if (++timer_a_counter_ >= (1024 - timer_a_)) {
                timer_a_counter_ = 0;
                timer_a_overflow_ = true;
                status_ |= 0x01;  // Timer A flag
            }
        }

        // Timer B: counts up, overflows at 256 (prescaled ×16)
        if (ctrl & ym_fm::fld::CH3_TIMER_EN_B) {
            if (++timer_b_counter_ >= ((256 - timer_b_) << 4)) {
                timer_b_counter_ = 0;
                timer_b_overflow_ = true;
                status_ |= 0x02;  // Timer B flag
            }
        }
    }

    // ========================================================================
    // LFO advancement
    // ========================================================================

    void advance_lfo() {
        // LFO rate table (approximate periods in master clocks)
        static constexpr uint16_t lfo_periods[8] = {
            108, 77, 71, 67, 62, 44, 8, 5
        };

        uint8_t rate = regs_[ym_fm::reg::LFO_REG] & ym_fm::fld::LFO_FREQ_LFO_RATE;
        bool enabled = (regs_[ym_fm::reg::LFO_REG] & ym_fm::fld::LFO_FREQ_LFO_EN) != 0;
        if (!enabled) return;

        if (++lfo_counter_ >= lfo_periods[rate]) {
            lfo_counter_ = 0;
            // Triangle-wave LFO for AM (0-126), sine-like for PM
            lfo_am_ = (lfo_am_ + 1) & 0x7F;
            lfo_pm_ = static_cast<int8_t>(ym_fm_tables::sine_table[
                (lfo_am_ << 3) & (ym_fm_tables::SINE_TABLE_SIZE - 1)] >> 8);
        }
    }

    // ========================================================================
    // FM sample generation
    // ========================================================================

    void generate_fm_sample() {
        float mix = 0.f;
        int active_channels = 0;

        for (int ch = 0; ch < NUM_FM_CH; ch++) {
            auto& c = channel_[ch];

            // DAC mode: channel 6 (index 5) outputs DAC value directly
            if constexpr (has_dac()) {
                if (ch == 5 && dac_enabled_) {
                    c.output = (static_cast<int32_t>(dac_value_) - 128) << 6;
                    mix += static_cast<float>(c.output);
                    active_channels++;
                    continue;
                }
            }

            // Compute LFO modulation values for this channel
            uint16_t am_mod = 0;
            int32_t pm_fraction = 0;
            if constexpr (has_lfo()) {
                // AM: attenuation scaled by channel AMS sensitivity
                // ams_shift: {31=off, 3, 1, 0} → lfo_am >> shift
                static constexpr uint8_t ams_shift[4] = { 31, 3, 1, 0 };
                am_mod = static_cast<uint16_t>(lfo_am_ >> ams_shift[c.ams]);

                // PM: proportional frequency deviation scaled by PMS
                static constexpr int16_t pms_depth[8] = { 0, 1, 2, 3, 4, 6, 12, 24 };
                pm_fraction = static_cast<int32_t>(lfo_pm_) * pms_depth[c.pms];
            }

            // Advance operators with LFO modulation
            for (int op = 0; op < NUM_OPS; op++) {
                advance_operator(c.ops[op], am_mod, pm_fraction);
            }

            // Route through algorithm
            c.output = compute_algorithm(c);
            mix += static_cast<float>(c.output);
            active_channels++;
        }

        // Normalize to [-1.0, +1.0]
        if (active_channels > 0) {
            // 8191 max per channel × N channels → normalize
            last_sample_ = mix / (8191.f * active_channels);

            // YM2612 (NMOS) ladder-effect DAC distortion: the 9-bit internal
            // DAC has non-uniform step sizes due to the NMOS process.  The most
            // audible artifact is a zero-crossing discontinuity where positive
            // values are offset by ~1 LSB relative to negative values.
            // The YM3438 (CMOS) corrected this, producing a cleaner output.
            if constexpr (Traits.ladder_effect) {
                if (last_sample_ > 0.f) {
                    last_sample_ += 1.f / 512.f;  // +1 LSB of 9-bit DAC
                }
            }
        }
    }

    // ========================================================================
    // Operator advancement — phase + envelope
    // ========================================================================

    void advance_operator(FMOperator& op, uint16_t am_mod, int32_t pm_fraction) {
        // --- Phase generator ---
        uint32_t mul = op.mul ? op.mul : 1;  // MUL=0 → ×½ (we handle as ×1 with shift)
        int32_t phase_inc = static_cast<int32_t>(op.freq * mul);
        if (op.mul == 0) phase_inc >>= 1;    // MUL=0 means ×0.5

        // Apply LFO phase modulation (PM): proportional frequency deviation
        if (pm_fraction != 0) {
            phase_inc += (phase_inc * pm_fraction) >> 10;
        }
        if (phase_inc < 0) phase_inc = 0;

        op.phase += static_cast<uint32_t>(phase_inc);

        // --- Envelope generator ---
        advance_envelope(op);

        // --- Compute operator output ---
        uint32_t phase_idx = (op.phase >> (ym_fm_constants::PHASE_BITS -
                              ym_fm_tables::SINE_TABLE_BITS))
                          & (ym_fm_tables::SINE_TABLE_SIZE - 1);

        // Waveform select (OPL2+): 0=sine, 1=half-sine, 2=abs-sine, 3=quarter-sine
        int32_t sine_val;
        if constexpr (has_waveform_sel()) {
            switch (op.waveform & 0x03) {
                default:
                case 0: sine_val = ym_fm_tables::sine_table[phase_idx]; break;
                case 1: sine_val = (phase_idx < 512)
                                 ? ym_fm_tables::sine_table[phase_idx] : 0; break;
                case 2: sine_val = ym_fm_tables::sine_table[phase_idx & 0x1FF]; break;
                case 3: sine_val = (phase_idx & 0x100)
                                 ? 0 : ym_fm_tables::sine_table[phase_idx & 0xFF]; break;
            }
        } else {
            sine_val = ym_fm_tables::sine_table[phase_idx];
        }

        // Apply envelope attenuation (linear multiply)
        // SSG-EG inversion: when active and inverted, flip the envelope level
        uint16_t eff_env = op.env_level;
        if ((op.ssg_eg & 0x08) && op.ssg_inverted) {
            eff_env = ym_fm_constants::ENV_MAX - op.env_level;
        }
        uint16_t env = eff_env + (static_cast<uint16_t>(op.tl) << ym_fm_constants::TL_SHIFT);
        if (op.am_en) env += am_mod;  // LFO amplitude modulation
        if (env > ym_fm_constants::ENV_MAX) env = ym_fm_constants::ENV_MAX;

        // Attenuation: 0 = full volume, ENV_MAX = silence
        int32_t attenuation = ym_fm_constants::ENV_MAX - env;
        op.prev_output = op.output;
        op.output = (sine_val * attenuation) >> ym_fm_constants::ENV_BITS;
    }

    // ========================================================================
    // Envelope generator
    // ========================================================================
    //
    // SHORTCOMING: This is a rough linear approximation.  Real hardware uses
    // a rate counter with per-rate increment tables (4 increments per rate,
    // cycled by a global counter).  Attack is exponential (level += ~level*rate),
    // decay/release are linear in log domain.

    void advance_envelope(FMOperator& op) {
        // Rate-scaling: higher notes progress through the envelope faster.
        // effective_rate = 2 * base_rate + (keycode >> (3 - rs)), clamped to 63.
        auto scaled_rate = [&](uint8_t base_rate) -> uint8_t {
            if (base_rate == 0) return 0;
            uint8_t eff = base_rate * 2 + (op.keycode >> (3 - op.rs));
            return eff > 63 ? static_cast<uint8_t>(63) : eff;
        };

        switch (op.env_state) {
            case FMOperator::OFF:
                op.env_level = ym_fm_constants::ENV_MAX;
                break;

            case FMOperator::ATTACK: {
                uint8_t rate = scaled_rate(op.ar);
                if (rate >= 62) {
                    op.env_level = 0;
                    op.env_state = FMOperator::DECAY1;
                } else if (rate > 0) {
                    // Exponential attack: level += (~level * rate) >> shift
                    uint16_t step = ((ym_fm_constants::ENV_MAX - op.env_level)
                                    * static_cast<uint16_t>(rate)) >> 4;
                    if (step == 0) step = 1;
                    if (op.env_level > step)
                        op.env_level -= step;
                    else {
                        op.env_level = 0;
                        op.env_state = FMOperator::DECAY1;
                    }
                }
                break;
            }

            case FMOperator::DECAY1: {
                uint16_t target = static_cast<uint16_t>(op.d1l) << 5;  // D1L × 32
                uint8_t rate = scaled_rate(op.d1r);
                if (rate > 0) {
                    op.env_level += rate;
                    if (op.env_level >= target) {
                        op.env_level = target;
                        op.env_state = FMOperator::DECAY2;
                    }
                }
                break;
            }

            case FMOperator::DECAY2: {
                uint8_t rate = scaled_rate(op.d2r);
                if (rate > 0) {
                    op.env_level += rate;
                    if (op.env_level >= ym_fm_constants::ENV_MAX)
                        op.env_level = ym_fm_constants::ENV_MAX;
                }
                break;
            }

            case FMOperator::RELEASE: {
                uint8_t rate = scaled_rate(op.rr);
                if (rate > 0) {
                    uint16_t step = (rate << 1) + 1;
                    op.env_level += step;
                    if (op.env_level >= ym_fm_constants::ENV_MAX) {
                        op.env_level = ym_fm_constants::ENV_MAX;
                        op.env_state = FMOperator::OFF;
                    }
                }
                break;
            }
        }

        // SSG-EG: when enabled, alter envelope shape on reaching max attenuation.
        // Bits 2:0 encode shape (attack-invert / alternate / hold).
        if ((op.ssg_eg & 0x08) && op.env_state != FMOperator::OFF
                               && op.env_state != FMOperator::ATTACK) {
            if (op.env_level >= ym_fm_constants::ENV_MAX) {
                if (op.ssg_eg & 0x01) {
                    // Hold: stop cycling, optionally toggle inversion
                    if (op.ssg_eg & 0x02) op.ssg_inverted = !op.ssg_inverted;
                    op.env_level = 0;
                    op.env_state = FMOperator::OFF;
                } else {
                    // Loop: restart envelope from attack
                    op.env_level = 0;
                    op.env_state = FMOperator::ATTACK;
                    if (op.ssg_eg & 0x02) op.ssg_inverted = !op.ssg_inverted;
                }
            }
        }
    }

    // ========================================================================
    // Algorithm routing — 4-op (algorithms 0-7) or 2-op (algorithms 0-3)
    // ========================================================================
    //
    // SHORTCOMING: This is the most critical gap.  The algorithm diagrams
    // below show modulator→carrier signal flow, but the current
    // implementation advances all operators independently and then
    // compute_Xop_algorithm() simply picks which *pre-computed* outputs
    // to sum.  No operator's output is fed into another operator's phase.
    // This means:
    //   - Algorithms 0-6 all collapse to additive mixing of the selected
    //     outputs, producing no FM timbral character whatsoever.
    //   - Only algorithm 7 (all carriers, no modulation) is correct.
    //   - Feedback on op1 does modulate its own phase, but that's the
    //     only inter-sample modulation present.
    //
    // FIX: Evaluate operators in algorithm-dependent order, passing
    // each modulator's output into the next operator's phase_inc before
    // computing the sine lookup.
    //
    // 4-op algorithms (OPN/OPM):
    //   0: [op1→op2→op3→op4]→out
    //   1: [op1+op2]→op3→op4→out
    //   2: [op1+(op2→op3)]→op4→out
    //   3: [(op1→op2)+op3]→op4→out
    //   4: [(op1→op2)+(op3→op4)]→out
    //   5: [op1→(op2+op3+op4)]→out
    //   6: [(op1→op2)+op3+op4]→out
    //   7: [op1+op2+op3+op4]→out
    //
    // 2-op algorithms (OPL):
    //   0: [op1→op2]→out  (FM)
    //   1: [op1+op2]→out  (additive)

    int32_t compute_algorithm(FMChannel& c) {
        auto& op = c.ops;

        // Apply self-feedback to operator 1
        if (c.feedback > 0) {
            int32_t fb = (op[0].output + op[0].prev_output) >> (9 - c.feedback);
            // Modulate op[0]'s phase by feedback
            op[0].phase += static_cast<uint32_t>(fb) << (ym_fm_constants::PHASE_BITS -
                           ym_fm_tables::SINE_TABLE_BITS);
        }

        if constexpr (NUM_OPS == 4) {
            return compute_4op_algorithm(c);
        } else {
            return compute_2op_algorithm(c);
        }
    }

    int32_t compute_4op_algorithm(FMChannel& c) {
        auto& op = c.ops;
        switch (c.algorithm) {
            case 0:  return op[3].output;  // Serial: 1→2→3→4
            case 1:  return op[3].output;  // (1+2)→3→4
            case 2:  return op[3].output;  // (1+(2→3))→4
            case 3:  return op[3].output;  // ((1→2)+3)→4
            case 4:  return op[1].output + op[3].output;  // (1→2)+(3→4)
            case 5:  return op[1].output + op[2].output + op[3].output;  // 1→(2+3+4)
            case 6:  return op[1].output + op[2].output + op[3].output;  // (1→2)+3+4
            case 7:  return op[0].output + op[1].output +
                            op[2].output + op[3].output;  // 1+2+3+4
            default: return 0;
        }
    }

    int32_t compute_2op_algorithm(FMChannel& c) {
        auto& op = c.ops;
        switch (c.algorithm) {
            case 0:  return op[1].output;              // FM: 1→2
            case 1:  return op[0].output + op[1].output; // Additive: 1+2
            default: return op[1].output;
        }
    }

    // ========================================================================
    // Register write handlers
    // ========================================================================

    void on_global_write(uint8_t addr, uint8_t data) {
        switch (addr) {
            case ym_fm::reg::LFO_REG:
                // LFO config — decoded directly from regs_ in advance_lfo()
                break;

            case ym_fm::reg::TIMER_A_H_REG:
                timer_a_ = (timer_a_ & 0x03) | (static_cast<uint16_t>(data) << 2);
                break;

            case ym_fm::reg::TIMER_A_L_REG:
                timer_a_ = (timer_a_ & 0x3FC) | (data & 0x03);
                break;

            case ym_fm::reg::TIMER_B_REG:
                timer_b_ = data;
                break;

            case ym_fm::reg::CH3_TIMER_REG:
                // Timer control — reset flags if requested
                if (data & ym_fm::fld::CH3_TIMER_RST_A) {
                    timer_a_overflow_ = false;
                    status_ &= ~0x01;
                }
                if (data & ym_fm::fld::CH3_TIMER_RST_B) {
                    timer_b_overflow_ = false;
                    status_ &= ~0x02;
                }
                break;

            case ym_fm::reg::KEY_ONOFF_ADDR: {
                uint8_t ch_idx = data & 0x07;
                // OPN2: channels 0-2 in bank 0, channels 3-5 mapped via bit 2
                if (ch_idx >= 3 && ch_idx < 4) break;  // Invalid
                if (ch_idx >= 4) ch_idx = ch_idx - 4 + 3;  // 4→3, 5→4, 6→5
                if (ch_idx >= NUM_FM_CH) break;

                auto& ch = channel_[ch_idx];
                for (int op = 0; op < NUM_OPS; op++) {
                    bool new_key = (data & (0x10 << op)) != 0;
                    if (new_key && !ch.ops[op].key_on) {
                        // Key ON → start attack
                        ch.ops[op].key_on = true;
                        ch.ops[op].env_state = FMOperator::ATTACK;
                        ch.ops[op].phase = 0;
                        // SSG-EG: ATT bit sets initial inversion on key-on
                        ch.ops[op].ssg_inverted = (ch.ops[op].ssg_eg & 0x0C) == 0x0C;
                    } else if (!new_key && ch.ops[op].key_on) {
                        // Key OFF → start release
                        ch.ops[op].key_on = false;
                        ch.ops[op].env_state = FMOperator::RELEASE;
                    }
                }
                break;
            }

            case ym_fm::reg::DAC_DATA_REG:
                if constexpr (has_dac()) {
                    dac_value_ = data;
                }
                break;

            case ym_fm::reg::DAC_EN_REG:
                if constexpr (has_dac()) {
                    dac_enabled_ = (data & ym_fm::fld::DAC_EN_DAC_ENABLE) != 0;
                }
                break;
        }
    }

    void on_operator_write(uint8_t addr, uint8_t data, uint8_t bank) {
        // Operator register layout: addr = base + (op_slot * 4 + ch_in_bank)
        // op_slot: 0-3, ch_in_bank: 0-2
        uint8_t ch_in_bank = addr & 0x03;
        if (ch_in_bank >= 3) return;  // Addr & 3 == 3 is unused

        uint8_t op_slot = ((addr - 0x30) >> 2) & 0x03;
        uint8_t base    = addr & 0xF0;

        // Resolve global channel index
        uint8_t ch_idx = ch_in_bank + (bank * 3);
        if (ch_idx >= NUM_FM_CH) return;

        // OPN operator ordering: slot 0→op1, slot 1→op3, slot 2→op2, slot 3→op4
        // (hardware interleave)
        static constexpr uint8_t op_map[4] = { 0, 2, 1, 3 };
        uint8_t op_idx = (NUM_OPS == 4) ? op_map[op_slot] : op_slot;
        if (op_idx >= NUM_OPS) return;

        auto& op = channel_[ch_idx].ops[op_idx];

        switch (base) {
            case 0x30:  // DT1/MUL
                op.dt1 = (data >> 4) & 0x07;
                op.mul = data & 0x0F;
                update_op_freq(ch_idx, op_idx);
                break;
            case 0x40:  // TL
                op.tl = data & 0x7F;
                break;
            case 0x50:  // RS/AR
                op.rs = (data >> 6) & 0x03;
                op.ar = data & 0x1F;
                break;
            case 0x60:  // AM/D1R
                op.am_en = (data & 0x80) != 0;
                op.d1r = data & 0x1F;
                break;
            case 0x70:  // D2R
                op.d2r = data & 0x1F;
                break;
            case 0x80:  // D1L/RR
                op.d1l = (data >> 4) & 0x0F;
                op.rr = data & 0x0F;
                break;
            case 0x90:  // SSG-EG
                op.ssg_eg = data & 0x0F;
                break;
        }
    }

    void on_channel_write(uint8_t addr, uint8_t data, uint8_t bank) {
        uint8_t ch_in_bank = addr & 0x03;
        if (ch_in_bank >= 3) return;

        uint8_t ch_idx = ch_in_bank + (bank * 3);
        if (ch_idx >= NUM_FM_CH) return;

        auto& ch = channel_[ch_idx];

        if (addr >= 0xA0 && addr < 0xA4) {
            // F-Num low 8 bits
            ch.fnum = (ch.fnum & 0x700) | data;
            update_channel_freq(ch_idx);
        } else if (addr >= 0xA4 && addr < 0xA8) {
            // Block + F-Num high 3 bits
            ch.block = (data >> 3) & 0x07;
            ch.fnum  = (ch.fnum & 0x0FF) | (static_cast<uint16_t>(data & 0x07) << 8);
            update_channel_freq(ch_idx);
        } else if (addr >= 0xA8 && addr < 0xAC) {
            // Ch3 special mode: per-operator F-Num low (slots 0-2)
            if constexpr (has_ch3_special()) {
                uint8_t slot = addr - 0xA8;
                ch3_fnum_[slot] = (ch3_fnum_[slot] & 0x700) | data;
                update_channel_freq(2);
            }
        } else if (addr >= 0xAC && addr < 0xB0) {
            // Ch3 special mode: per-operator Block/F-Num high (slots 0-2)
            if constexpr (has_ch3_special()) {
                uint8_t slot = addr - 0xAC;
                ch3_block_[slot] = (data >> 3) & 0x07;
                ch3_fnum_[slot] = (ch3_fnum_[slot] & 0x0FF)
                                | (static_cast<uint16_t>(data & 0x07) << 8);
                update_channel_freq(2);
            }
        } else if (addr >= 0xB0 && addr < 0xB4) {
            // FB/Algorithm
            ch.feedback  = (data >> 3) & 0x07;
            ch.algorithm = data & 0x07;
        } else if (addr >= 0xB4 && addr < 0xB8) {
            // L/R/AMS/PMS
            ch.left  = (data & 0x80) != 0;
            ch.right = (data & 0x40) != 0;
            ch.ams   = (data >> 4) & 0x03;
            ch.pms   = data & 0x07;
        }
    }

    // ========================================================================
    // Frequency calculation
    // ========================================================================

    void update_channel_freq(uint8_t ch_idx) {
        auto& ch = channel_[ch_idx];

        // Ch3 special mode: per-operator frequencies from supplementary registers.
        // Slots 0-2 use ch3_fnum_/ch3_block_; slot 3 uses normal channel freq.
        if constexpr (has_ch3_special()) {
            if (ch_idx == 2 && (regs_[ym_fm::reg::CH3_TIMER_REG]
                                & ym_fm::fld::CH3_TIMER_CH3_MODE)) {
                static constexpr uint8_t slot_to_op[3] = { 0, 2, 1 };
                for (int slot = 0; slot < 3; slot++) {
                    update_op_freq(2, slot_to_op[slot],
                                   ch3_fnum_[slot], ch3_block_[slot]);
                }
                update_op_freq(2, 3, ch.fnum, ch.block);
                return;
            }
        }

        for (int op = 0; op < NUM_OPS; op++) {
            update_op_freq(ch_idx, op, ch.fnum, ch.block);
        }
    }

    void update_op_freq(uint8_t ch_idx, uint8_t op_idx) {
        update_op_freq(ch_idx, op_idx,
                       channel_[ch_idx].fnum, channel_[ch_idx].block);
    }

    void update_op_freq(uint8_t ch_idx, uint8_t op_idx,
                        uint16_t fnum, uint8_t block) {
        auto& op = channel_[ch_idx].ops[op_idx];
        uint32_t base_freq = static_cast<uint32_t>(fnum) << block;

        // DT1 detune via hardware-accurate 32-entry LUT keyed by keycode.
        // keycode = (block << 2) | (fnum >> 9).  DT2 (OPM only) not yet implemented.
        op.keycode = (block << 2) | ((fnum >> 9) & 0x03);
        uint8_t dt_mag = op.dt1 & 0x03;
        int32_t detune = ym_fm_constants::DT1_LUT[dt_mag][op.keycode];
        if (op.dt1 & 0x04) detune = -detune;  // Bit 2 = sign

        op.freq = static_cast<uint32_t>(static_cast<int32_t>(base_freq) + detune);
    }
};
