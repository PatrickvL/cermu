#pragma once
/*
 * nes_apu.h - NES APU (Audio Processing Unit) Implementation
 *
 * Contains the APU class (ChipBase subclass) modelling the audio subsystem
 * integrated into the Ricoh 2A03 (NTSC) / 2A07 (PAL) CPU package.
 * Five channels: 2 pulse, 1 triangle, 1 noise, 1 DMC (delta modulation).
 *
 * The CPU integrates the APU via the apu_mixin in fam65xx_mixins.hpp.
 * GUI rendering lives in nes_apu_gui.cpp (same directory).
 */

#include <array>
#include <cmath>
#include <cstring>

#include <cstdint>

#include "chip/sound/sound_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include "core/system_lines.hpp"

// ============================================================================
// NES APU REGISTER TABLE — single source of truth (offsets from $4000)
// ============================================================================
//
// $4000-$4003 — Pulse 1,  $4004-$4007 — Pulse 2,
// $4008-$400B — Triangle, $400C-$400F — Noise,
// $4010-$4013 — DMC,      $4015 — Status (R/W), $4017 — Frame Counter (W)
// ($4014 = OAM DMA, $4016 = Controller — not APU registers)

#define NES_APU_DECL(REG, FLD, CMP)                                            \
    /* ── Pulse 1 ($4000-$4003) ─────────────────────────────── */             \
    REG(0x00, SQ1_VOL,    "Pulse 1 duty/vol/env")                             \
      FLD(SQ1_VOL, DUTY,       7:6, "Duty cycle",         Value, 0, 0)        \
      FLD(SQ1_VOL, LC_HALT,    5:5, "Length halt / env loop", Flag, 0, 0)     \
      FLD(SQ1_VOL, CONST_VOL,  4:4, "Constant volume",    Flag, 0, 0)        \
      FLD(SQ1_VOL, VOL_PERIOD, 3:0, "Volume / env period", Value, 0, 0)      \
    REG(0x01, SQ1_SWEEP,  "Pulse 1 sweep")                                    \
      FLD(SQ1_SWEEP, SW_EN,    7:7, "Sweep enable",       Flag, 0, 0)        \
      FLD(SQ1_SWEEP, SW_PER,   6:4, "Sweep period",       Value, 0, 0)       \
      FLD(SQ1_SWEEP, SW_NEG,   3:3, "Sweep negate",       Flag, 0, 0)        \
      FLD(SQ1_SWEEP, SW_SHIFT, 2:0, "Sweep shift",        Value, 0, 0)       \
    REG(0x02, SQ1_LO,     "Pulse 1 timer low")                                \
    REG(0x03, SQ1_HI,     "Pulse 1 timer hi / length")                        \
      FLD(SQ1_HI, LC_LOAD,    7:3, "Length counter load", Value, 0, 0)        \
      FLD(SQ1_HI, TIMER_HI,   2:0, "Timer high bits",    Value, 0, 0)        \
    /* ── Pulse 2 ($4004-$4007) ─────────────────────────────── */             \
    REG(0x04, SQ2_VOL,    "Pulse 2 duty/vol/env")                             \
      FLD(SQ2_VOL, DUTY,       7:6, "Duty cycle",         Value, 0, 0)        \
      FLD(SQ2_VOL, LC_HALT,    5:5, "Length halt / env loop", Flag, 0, 0)     \
      FLD(SQ2_VOL, CONST_VOL,  4:4, "Constant volume",    Flag, 0, 0)        \
      FLD(SQ2_VOL, VOL_PERIOD, 3:0, "Volume / env period", Value, 0, 0)      \
    REG(0x05, SQ2_SWEEP,  "Pulse 2 sweep")                                    \
      FLD(SQ2_SWEEP, SW_EN,    7:7, "Sweep enable",       Flag, 0, 0)        \
      FLD(SQ2_SWEEP, SW_PER,   6:4, "Sweep period",       Value, 0, 0)       \
      FLD(SQ2_SWEEP, SW_NEG,   3:3, "Sweep negate",       Flag, 0, 0)        \
      FLD(SQ2_SWEEP, SW_SHIFT, 2:0, "Sweep shift",        Value, 0, 0)       \
    REG(0x06, SQ2_LO,     "Pulse 2 timer low")                                \
    REG(0x07, SQ2_HI,     "Pulse 2 timer hi / length")                        \
      FLD(SQ2_HI, LC_LOAD,    7:3, "Length counter load", Value, 0, 0)        \
      FLD(SQ2_HI, TIMER_HI,   2:0, "Timer high bits",    Value, 0, 0)        \
    /* ── Triangle ($4008-$400B) ────────────────────────────── */              \
    REG(0x08, TRI_LINEAR, "Triangle linear counter")                           \
      FLD(TRI_LINEAR, LC_HALT, 7:7, "Length halt / lin reload", Flag, 0, 0)   \
      FLD(TRI_LINEAR, LIN_LOAD, 6:0, "Linear counter load", Value, 0, 0)     \
    REG(0x09, TRI_UNUSED, "Triangle unused")                                   \
    REG(0x0A, TRI_LO,     "Triangle timer low")                                \
    REG(0x0B, TRI_HI,     "Triangle timer hi / length")                        \
      FLD(TRI_HI, LC_LOAD,    7:3, "Length counter load", Value, 0, 0)        \
      FLD(TRI_HI, TIMER_HI,   2:0, "Timer high bits",    Value, 0, 0)        \
    /* ── Noise ($400C-$400F) ───────────────────────────────── */              \
    REG(0x0C, NOISE_VOL,  "Noise vol/env")                                     \
      FLD(NOISE_VOL, LC_HALT,    5:5, "Length halt / env loop", Flag, 0, 0)   \
      FLD(NOISE_VOL, CONST_VOL,  4:4, "Constant volume",    Flag, 0, 0)      \
      FLD(NOISE_VOL, VOL_PERIOD, 3:0, "Volume / env period", Value, 0, 0)    \
    REG(0x0D, NOISE_UNUSED, "Noise unused")                                    \
    REG(0x0E, NOISE_LO,   "Noise mode / period")                               \
      FLD(NOISE_LO, MODE,     7:7, "Noise mode (short)",  Flag, 0, 0)        \
      FLD(NOISE_LO, PERIOD,   3:0, "Noise period index",  Value, 0, 0)       \
    REG(0x0F, NOISE_HI,   "Noise length load")                                 \
      FLD(NOISE_HI, LC_LOAD,  7:3, "Length counter load", Value, 0, 0)        \
    /* ── DMC ($4010-$4013) ─────────────────────────────────── */              \
    REG(0x10, DMC_FREQ,   "DMC flags / rate")                                  \
      FLD(DMC_FREQ, IRQ_EN,    7:7, "IRQ enable",         Flag, 0, 0)        \
      FLD(DMC_FREQ, LOOP,      6:6, "Loop",               Flag, 0, 0)        \
      FLD(DMC_FREQ, RATE_IDX,  3:0, "Rate index",         Value, 0, 0)       \
    REG(0x11, DMC_RAW,    "DMC direct load")                                   \
      FLD(DMC_RAW, LOAD,       6:0, "Direct load value",  Value, 0, 0)       \
    REG(0x12, DMC_START,  "DMC sample address")                                \
    REG(0x13, DMC_LEN,    "DMC sample length")                                 \
    /* ── $4014 = OAM DMA (not APU) ─────────────────────────── */             \
    REG(0x14, OAM_DMA,    "[not APU] OAM DMA page")                           \
    /* ── Status ($4015) ────────────────────────────────────── */              \
    REG(0x15, STATUS,     "Status (R: flags / W: enable)")                     \
      FLD(STATUS, DMC_EN,      4:4, "DMC enable",         Flag, 0, 0)        \
      FLD(STATUS, NOISE_EN,    3:3, "Noise enable",       Flag, 0, 0)        \
      FLD(STATUS, TRI_EN,      2:2, "Triangle enable",    Flag, 0, 0)        \
      FLD(STATUS, SQ2_EN,      1:1, "Pulse 2 enable",     Flag, 0, 0)        \
      FLD(STATUS, SQ1_EN,      0:0, "Pulse 1 enable",     Flag, 0, 0)        \
    /* ── $4016 = Controller (not APU) ──────────────────────── */              \
    REG(0x16, JOY1,       "[not APU] Controller 1")                            \
    /* ── Frame Counter ($4017) ─────────────────────────────── */              \
    REG(0x17, FRAME_CNT,  "Frame counter (W only)")                            \
      FLD(FRAME_CNT, MODE,     7:7, "Sequencer mode (5-step)", Flag, 0, 0)   \
      FLD(FRAME_CNT, IRQ_INH,  6:6, "IRQ inhibit",        Flag, 0, 0)

namespace nes_apu {
namespace reg {
    NES_APU_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr uint8_t REG_COUNT = 0x18;
}
namespace fld {
#define NES_APU_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
NES_APU_DECL(DECL_REG_NOP, NES_APU_X_FLD_NS_, DECL_CMP_NOP)
#undef NES_APU_X_FLD_NS_
} // namespace fld
} // namespace nes_apu

DECL_EXTRACT(NES_APU, NES_APU_DECL)

// ============================================================================
// INTEGRATED APU IMPLEMENTATION (C++)
// ============================================================================

namespace nes6502_apu {

// APU Constants
constexpr uint32_t CPU_FREQ_NTSC = 1789773;
constexpr uint32_t CPU_FREQ_PAL = 1662607;

} // namespace nes6502_apu

// Length counter lookup table
constexpr uint8_t APU_LENGTH_TABLE[32] = {
    10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60,  10, 14, 12, 26, 14,
    30, 16,  12, 18, 24, 20, 48, 22, 96,  24, 192, 26, 72, 28, 16, 30,
};

// Noise channel period lookup tables
constexpr uint16_t NOISE_PERIOD_NTSC[16] = {4,   8,   16,  32,  64,  96,
                                            128, 160, 202, 254, 380, 508,
                                            762, 1016, 2034, 4068};
constexpr uint16_t NOISE_PERIOD_PAL[16] = {4,   8,   14,  30,  60,   88,
                                           118, 148, 188, 236, 354,  472,
                                           708, 944, 1890, 3778};

// DMC channel rate lookup tables
constexpr uint16_t DMC_PERIOD_NTSC[16] = {428, 380, 340, 320, 286, 254, 226, 214,
                                          190, 160, 142, 128, 106,  84,  72,  54};
constexpr uint16_t DMC_PERIOD_PAL[16] = {398, 354, 316, 298, 276, 236, 210, 198,
                                         176, 148, 132, 118,  98,  78,  66,  50};

// Duty cycle sequences for pulse channels
constexpr uint8_t DUTY_TABLE[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0}, // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0}, // 25%
    {0, 1, 1, 1, 1, 0, 0, 0}, // 50%
    {1, 0, 0, 1, 1, 1, 1, 1}, // 75% (negated 25%)
};

// Triangle channel waveform
constexpr uint8_t TRIANGLE_TABLE[32] = {15, 14, 13, 12, 11, 10, 9,  8,
                                        7,  6,  5,  4,  3,  2,  1,  0,
                                        0,  1,  2,  3,  4,  5,  6,  7,
                                        8,  9,  10, 11, 12, 13, 14, 15};

// ============================================================================
// Mixer lookup tables (NESdev wiki: https://www.nesdev.org/wiki/APU_Mixer)
//
// Pulse:  pulse_table[p1 + p2]  (exact, 31 entries)
//   pulse_table[n] = 95.52 / (8128.0 / n + 100.0)  for n>0, 0 for n=0
//
// TND:    tnd_table[3*tri + 2*noi + dmc]  (linear approximation, 203 entries)
//   tnd_table[n] = 163.67 / (24329.0 / n + 100.0)   for n>0, 0 for n=0
// ============================================================================
namespace apu_mixer {

inline constexpr auto make_pulse_table() {
    std::array<float, 31> t{};
    t[0] = 0.0f;
    for (int n = 1; n < 31; n++) {
        t[n] = 95.52f / (8128.0f / n + 100.0f);
    }
    return t;
}

inline constexpr auto make_tnd_table() {
    std::array<float, 203> t{};
    t[0] = 0.0f;
    for (int n = 1; n < 203; n++) {
        t[n] = 163.67f / (24329.0f / n + 100.0f);
    }
    return t;
}

inline constexpr auto pulse_table = make_pulse_table();
inline constexpr auto tnd_table = make_tnd_table();

} // namespace apu_mixer

namespace nes6502_apu {

// ============================================================================
// Envelope Generator
// ============================================================================
class Envelope {
public:
  bool start = false;
  bool loop = false;
  bool constant_volume = false;
  uint8_t divider_period = 0;
  uint8_t constant_value = 0;

private:
  uint8_t decay_counter = 0;
  uint8_t divider = 0;

public:
  void reset() {
    start = true;
  }

  void clock() {
    if (start) {
      start = false;
      decay_counter = 15;
      divider = divider_period;
    } else if (divider == 0) {
      divider = divider_period;
      if (decay_counter > 0) {
        decay_counter--;
      } else if (loop) {
        decay_counter = 15;
      }
    } else {
      divider--;
    }
  }

  uint8_t volume() const {
    return constant_volume ? constant_value : decay_counter;
  }
};

// ============================================================================
// Length Counter
// ============================================================================
class LengthCounter {
public:
  bool enabled = false;
  bool halt = false;

private:
  uint8_t counter = 0;

  // The halt flag used for the length clock is from the PREVIOUS cycle.
  // On real hardware, the half-frame clock fires on the leading edge of
  // φ1 and uses the latch state from the previous cycle, while register
  // writes (which update halt) complete later on the same φ1 cycle.
  // Result: "changes to halt occur after clocking length" (blargg test 10).
  bool prev_halt_ = false;

  // Pending reload: when a write to the length register ($4003/$4007/
  // $400B/$400F) occurs, the reload is buffered.  APU::tick() resolves
  // it after the half-frame clock (if any).  On a half-frame cycle, the
  // reload is silently dropped when the post-clock counter is > 0
  // (blargg test 11 #5).  On non-half-frame cycles, the reload always
  // takes effect.
  bool pending_reload_ = false;
  uint8_t pending_reload_index_ = 0;

public:
  void load(uint8_t index) {
    if (!enabled) return;
    // Buffer the reload — APU::tick() will resolve it with knowledge
    // of whether a half-frame clock happened this cycle.
    pending_reload_ = true;
    pending_reload_index_ = index;
  }

  void clock() {
    // Use PREVIOUS cycle's halt for the clock decision
    if (!prev_halt_ && counter > 0) {
      counter--;
    }
  }

  // Resolve any pending reload.  Called by APU::tick() at the end of
  // every cycle.  On half-frame cycles, the reload is dropped if the
  // counter is still > 0 after the clock.
  void resolve_pending_reload(bool had_half_frame) {
    if (pending_reload_) {
      if (!had_half_frame || counter == 0) {
        counter = APU_LENGTH_TABLE[pending_reload_index_];
      }
      // If had_half_frame && counter > 0: reload silently dropped
      pending_reload_ = false;
    }
  }

  // Called at the end of APU::tick() to latch halt for next cycle's clock.
  void update_prev_halt() { prev_halt_ = halt; }

  void reset() {
    counter = 0;
    pending_reload_ = false;
    pending_reload_index_ = 0;
    prev_halt_ = false;
    halt = false;
  }

  void set_enabled(bool enable) {
    enabled = enable;
    if (!enabled) {
      counter = 0;
    }
  }

  bool active() const { return counter > 0; }

  uint8_t value() const { return counter; }
};

// ============================================================================
// Sweep Unit (for Pulse channels)
// ============================================================================
class Sweep {
public:
  bool enabled = false;
  bool negate = false;
  bool reload = false;
  uint8_t shift = 0;
  uint8_t period = 0;
  bool is_pulse1 = false; // Pulse 1 uses one's complement

private:
  uint8_t divider = 0;

public:
  uint16_t calculate_target(uint16_t current_period) const {
    uint16_t change = current_period >> shift;
    if (negate) {
      // Pulse 1 uses one's complement (subtracts change + 1)
      // Pulse 2 uses two's complement (subtracts change)
      if (is_pulse1) {
        return current_period - change - 1;
      } else {
        return current_period - change;
      }
    } else {
      return current_period + change;
    }
  }

  bool is_muting(uint16_t current_period) const {
    return current_period < 8 || calculate_target(current_period) > 0x7FF;
  }

  void clock(uint16_t &current_period) {
    if (divider == 0 && enabled && shift > 0 && !is_muting(current_period)) {
      current_period = calculate_target(current_period);
    }

    if (divider == 0 || reload) {
      divider = period;
      reload = false;
    } else {
      divider--;
    }
  }

  void reset() {
    divider = 0;
    reload = false;
  }
};

// ============================================================================
// Pulse Channel
// ============================================================================
class PulseChannel {
public:
  Envelope envelope;
  LengthCounter length;
  Sweep sweep;

  uint8_t duty = 0;
  uint16_t timer_period = 0;

private:
  uint16_t timer = 0;
  uint8_t sequence_pos = 0;

public:
  PulseChannel(bool is_pulse1) { sweep.is_pulse1 = is_pulse1; }

  void write_control(uint8_t value) {
    duty = (value >> 6) & 0x3;
    length.halt = (value >> 5) & 1;
    envelope.loop = (value >> 5) & 1;
    envelope.constant_volume = (value >> 4) & 1;
    envelope.divider_period = value & 0x0F;
    envelope.constant_value = value & 0x0F;
  }

  void write_sweep(uint8_t value) {
    sweep.enabled = (value >> 7) & 1;
    sweep.period = (value >> 4) & 0x7;
    sweep.negate = (value >> 3) & 1;
    sweep.shift = value & 0x7;
    sweep.reload = true;
  }

  void write_timer_low(uint8_t value) {
    timer_period = (timer_period & 0x700) | value;
  }

  void write_timer_high(uint8_t value) {
    timer_period = (timer_period & 0xFF) | ((value & 0x7) << 8);
    length.load(value >> 3);
    sequence_pos = 0;
    envelope.reset();
  }

  void clock() {
    if (timer == 0) {
      timer = timer_period;
      sequence_pos = (sequence_pos + 1) & 0x7;
    } else {
      timer--;
    }
  }

  uint8_t output() const {
    if (!length.active())
      return 0;
    if (sweep.is_muting(timer_period))
      return 0;
    if (timer_period < 8)
      return 0;
    if (DUTY_TABLE[duty][sequence_pos] == 0)
      return 0;
    return envelope.volume();
  }
};

// ============================================================================
// Triangle Channel
// ============================================================================
class TriangleChannel {
public:
  LengthCounter length;

  bool control_flag = false;
  uint8_t linear_counter_load = 0;
  uint16_t timer_period = 0;

private:
  uint16_t timer = 0;
  uint8_t sequence_pos = 0;
  uint8_t linear_counter = 0;
  bool linear_reload = false;

public:
  void write_control(uint8_t value) {
    control_flag = (value >> 7) & 1;
    length.halt = control_flag;
    linear_counter_load = value & 0x7F;
  }

  void write_timer_low(uint8_t value) {
    timer_period = (timer_period & 0x700) | value;
  }

  void write_timer_high(uint8_t value) {
    timer_period = (timer_period & 0xFF) | ((value & 0x7) << 8);
    length.load(value >> 3);
    linear_reload = true;
  }

  void clock() {
    if (timer == 0) {
      timer = timer_period;
      // Only advance sequence if both counters are active
      if (length.active() && linear_counter > 0) {
        sequence_pos = (sequence_pos + 1) & 0x1F;
      }
    } else {
      timer--;
    }
  }

  void clock_linear_counter() {
    if (linear_reload) {
      linear_counter = linear_counter_load;
    } else if (linear_counter > 0) {
      linear_counter--;
    }

    if (!control_flag) {
      linear_reload = false;
    }
  }

  uint8_t output() const {
    return TRIANGLE_TABLE[sequence_pos];
  }

  uint8_t linear_counter_value() const { return linear_counter; }

  void reset() {
    sequence_pos = 0;
    linear_counter = 0;
    linear_reload = false;
    timer = 0;
  }
};

// ============================================================================
// Noise Channel
// ============================================================================
class NoiseChannel {
public:
  Envelope envelope;
  LengthCounter length;

  bool mode = false; // false = 15-bit, true = 6-bit (short mode)
  uint8_t period_index = 0;
  bool is_pal = false;
  uint16_t shift_register = 1; // LFSR starts at 1

private:
  uint16_t timer = 0;

public:
  void write_control(uint8_t value) {
    length.halt = (value >> 5) & 1;
    envelope.loop = (value >> 5) & 1;
    envelope.constant_volume = (value >> 4) & 1;
    envelope.divider_period = value & 0x0F;
    envelope.constant_value = value & 0x0F;
  }

  void write_period(uint8_t value) {
    mode = (value >> 7) & 1;
    period_index = value & 0x0F;
  }

  void write_length(uint8_t value) {
    length.load(value >> 3);
    envelope.reset();
  }

  void clock() {
    if (timer == 0) {
      timer = is_pal ? NOISE_PERIOD_PAL[period_index]
                     : NOISE_PERIOD_NTSC[period_index];

      // LFSR feedback: XOR bit 0 with bit 1 (normal) or bit 6 (short mode)
      uint8_t feedback_bit = mode ? 6 : 1;
      uint16_t feedback =
          (shift_register & 1) ^ ((shift_register >> feedback_bit) & 1);
      shift_register = (shift_register >> 1) | (feedback << 14);
    } else {
      timer--;
    }
  }

  uint8_t output() const {
    if (!length.active())
      return 0;
    // LFSR bit 0 = 1 means silence
    if (shift_register & 1)
      return 0;
    return envelope.volume();
  }
};

// ============================================================================
// DMC Channel
// ============================================================================
class DMCChannel {
public:
  bool irq_enabled = false;
  bool loop = false;
  uint8_t rate_index = 0;
  uint8_t output_level = 0;
  uint16_t sample_address = 0;
  uint16_t sample_length = 0;
  bool is_pal = false;

  // Public for APU access
  uint16_t current_address = 0;
  uint16_t bytes_remaining = 0;
  bool irq_flag = false;
  bool needs_sample = false; // Signal to CPU for DMA

private:
  uint16_t timer = 0;
  uint8_t sample_buffer = 0;
  bool sample_buffer_empty = true;
  uint8_t shift_register = 0;
  uint8_t bits_remaining = 0;
  bool silence = true;
  bool initial_fetch_ = false;

public:
  void write_control(uint8_t value) {
    irq_enabled = (value >> 7) & 1;
    loop = (value >> 6) & 1;
    rate_index = value & 0x0F;

    if (!irq_enabled) {
      irq_flag = false;
    }
  }

  void write_output(uint8_t value) {
    // Direct load — takes effect immediately
    output_level = value & 0x7F;
  }

  void write_address(uint8_t value) { sample_address = 0xC000 | (value << 6); }

  void write_length(uint8_t value) { sample_length = (value << 4) | 1; }

  void start() {
    current_address = sample_address;
    bytes_remaining = sample_length;

    if (bytes_remaining > 0 && sample_buffer_empty) {
      needs_sample = true;
      initial_fetch_ = true;  // First fetch gets special handling
    }
  }

  /// Called when DMA fetches a sample byte from memory.
  /// The initial fetch (triggered by start()) decrements bytes_remaining
  /// immediately so $4015 reports the correct state right away.
  /// The address counter is NOT advanced here — clock() handles that
  /// when the output unit consumes the buffer and requests the next fetch.
  void load_sample(uint8_t data) {
    sample_buffer = data;
    sample_buffer_empty = false;
    needs_sample = false;

    if (initial_fetch_) {
      initial_fetch_ = false;
      // Decrement bytes_remaining immediately so $4015 bit 4 reflects
      // the correct state right after start() (required for 1-byte
      // samples to show as inactive immediately).
      // NOTE: do NOT increment current_address here — clock() will
      // handle the address advance when the output unit consumes this
      // byte and requests the next fetch.
      if (bytes_remaining > 0) {
        bytes_remaining--;
      }
      if (bytes_remaining == 0) {
        if (loop) {
          start();
        } else if (irq_enabled) {
          irq_flag = true;
        }
      }
    }
  }

  void clock() {
    if (timer == 0) {
      // Table values are actual periods; reload with period-1 since
      // the counter counts from reload down to 0 (inclusive).
      timer = (is_pal ? DMC_PERIOD_PAL[rate_index] : DMC_PERIOD_NTSC[rate_index]) - 1;

      if (!silence) {
        if (shift_register & 1) {
          if (output_level <= 125)
            output_level += 2;
        } else {
          if (output_level >= 2)
            output_level -= 2;
        }
      }

      shift_register >>= 1;
      bits_remaining--;

      if (bits_remaining == 0) {
        bits_remaining = 8;
        if (sample_buffer_empty) {
          silence = true;
        } else {
          silence = false;
          shift_register = sample_buffer;
          sample_buffer_empty = true;

          // Request next byte and advance memory reader state.
          // bytes_remaining is decremented here for output-driven
          // fetches (not the initial fetch which is handled in
          // load_sample).
          if (bytes_remaining > 0) {
            needs_sample = true;
            current_address = (current_address + 1) | 0x8000;
            bytes_remaining--;

            if (bytes_remaining == 0) {
              if (loop) {
                start();
              } else if (irq_enabled) {
                irq_flag = true;
              }
            }
          }
        }
      }
    } else {
      timer--;
    }
  }

  uint8_t output() const {
    return output_level;
  }

  bool active() const { return bytes_remaining > 0; }

  void reset() {
    timer = (is_pal ? DMC_PERIOD_PAL[0] : DMC_PERIOD_NTSC[0]) - 1;
    sample_buffer = 0;
    sample_buffer_empty = true;
    shift_register = 0;
    bits_remaining = 8;
    silence = true;
    initial_fetch_ = false;
    output_level = 0;
    irq_flag = false;
    needs_sample = false;
    bytes_remaining = 0;
    current_address = 0xC000;
  }
};

// ============================================================================
// Frame Counter / Sequencer
//
// Table-driven: pre-built event schedules reduce the per-tick hot path from
// ~10 threshold comparisons to a single comparison against the next scheduled
// event cycle.  Events fire ~4× per frame counter period (~30K CPU ticks),
// so >99.98% of ticks take the fast path.
// ============================================================================
class FrameCounter {
public:
  bool mode = false; // false = 4-step, true = 5-step
  bool irq_inhibit = false;
  bool irq_flag = false;
  bool is_pal = false;

private:
  uint32_t cycle = 0;

  // Frame counter write delay (3-4 cycles)
  struct {
    bool pending = false;
    uint8_t value = 0;
    uint8_t delay = 0;
  } write_buffer;

  // --- Event schedule tables ---
  // Each entry: { cpu_cycle, flags }
  // Flags: bit 0 = quarter frame, bit 1 = half frame,
  //        bit 2 = IRQ trigger (conditional on !irq_inhibit),
  //        bit 3 = cycle reset (wrap to 0)
  static constexpr uint8_t EVT_QF    = 1;
  static constexpr uint8_t EVT_HF    = 2;
  static constexpr uint8_t EVT_IRQ   = 4;
  static constexpr uint8_t EVT_RESET = 8;

  struct Event { uint32_t cycle; uint8_t flags; };

  // 4-step NTSC: QF/HF at standard steps, IRQ 3-cycle window, reset at step3+1
  static constexpr Event SCHED_NTSC_4[] = {
      {7457,  EVT_QF},
      {14913, EVT_QF | EVT_HF},
      {22371, EVT_QF},
      {29828, EVT_IRQ},
      {29829, EVT_QF | EVT_HF | EVT_IRQ},
      {29830, EVT_IRQ | EVT_RESET},
  };
  // 5-step NTSC: QF/HF at steps 0,1,2,4; step 3 empty; no IRQ
  // Note: RESET is split to a separate cycle (37282) from the HF (37281)
  // so that post-reset events align correctly (same as mode 0's separate
  // HF at 29829 / RESET at 29830).
  static constexpr Event SCHED_NTSC_5[] = {
      {7457,  EVT_QF},
      {14913, EVT_QF | EVT_HF},
      {22371, EVT_QF},
      {37281, EVT_QF | EVT_HF},
      {37282, EVT_RESET},
  };
  // 4-step PAL
  static constexpr Event SCHED_PAL_4[] = {
      {8313,  EVT_QF},
      {16627, EVT_QF | EVT_HF},
      {24939, EVT_QF},
      {33252, EVT_IRQ},
      {33253, EVT_QF | EVT_HF | EVT_IRQ},
      {33254, EVT_IRQ | EVT_RESET},
  };
  // 5-step PAL
  static constexpr Event SCHED_PAL_5[] = {
      {8313,  EVT_QF},
      {16627, EVT_QF | EVT_HF},
      {24939, EVT_QF},
      {41565, EVT_QF | EVT_HF},
      {41566, EVT_RESET},
  };

  // Active schedule pointer and length
  const Event* schedule_ = SCHED_NTSC_4;
  uint8_t schedule_len_ = 6;
  uint8_t event_index_ = 0;

  // Select the active schedule based on mode and region
  void select_schedule() {
      if (mode) {
          if (is_pal) { schedule_ = SCHED_PAL_5; schedule_len_ = 5; }
          else        { schedule_ = SCHED_NTSC_5; schedule_len_ = 5; }
      } else {
          if (is_pal) { schedule_ = SCHED_PAL_4; schedule_len_ = 6; }
          else        { schedule_ = SCHED_NTSC_4; schedule_len_ = 6; }
      }
      // Find the first event after the current cycle
      event_index_ = 0;
      for (uint8_t i = 0; i < schedule_len_; i++) {
          if (schedule_[i].cycle > cycle) {
              event_index_ = i;
              return;
          }
      }
      event_index_ = 0; // Wrapped — next event is the first
  }

public:
  // On real hardware the APU frame counter begins running several cycles
  // before the CPU reset sequence finishes.  By the time the first user
  // instruction executes the counter has advanced 9-12 cycles past the
  // implicit $4017 write (the exact value varies per power-on; Blargg
  // tests report "usually 9").  Our CPU reset is instantaneous, so we
  // pre-seed the counter to compensate.
  //
  // Measured relationship: STARTUP_OFFSET = N  →  Blargg count = N + 3.
  // Acceptable count range: [6, 12]  →  OFFSET range: [3, 9].
  // "Usually 9" → OFFSET = 6.
  static constexpr uint32_t STARTUP_OFFSET = 6;

  /// Full reset (power-on): clears everything including write buffer.
  void power_on_reset() {
      cycle = STARTUP_OFFSET;
      irq_flag = false;
      write_buffer.pending = false;
      write_buffer.delay = 0;
      write_buffer.value = 0;
      mode = false;
      irq_inhibit = false;
      select_schedule();
  }

  /// Soft reset: restarts the frame counter with the same hardware
  /// startup offset as power-on (the real chip re-writes $4017 with
  /// the previously latched value and delays 9-12 cycles).
  void soft_reset() {
      cycle = STARTUP_OFFSET;
      irq_flag = false;
      write_buffer.pending = false;
      write_buffer.delay = 0;
      select_schedule();
  }

  void write(uint8_t value, bool apu_odd_cycle) {
      // Frame counter writes are delayed by 3-4 CPU cycles.
      // The delay depends on the APU's even/odd cycle, NOT the frame
      // counter's internal cycle.  Even APU cycle → 3 cycle delay,
      // odd → 4 cycle delay.
      //
      // Implementation note: frame.write() is called during PHI1
      // and clock() runs later in the SAME PHI1.  That first clock()
      // decrements the delay once before any new CPU cycle passes.
      // To get N actual CPU-cycle delays we set delay = N+0 because:
      //   tick 0 (same cycle): delay N→N-1, not zero yet
      //   tick 1: delay N-1→N-2
      //   ...
      //   tick N-1: delay 1→0, write takes effect
      // Total: N-1 additional CPU cycles after the write cycle, PLUS
      //        the write cycle itself = N CPU cycles from STA to effect.
      write_buffer.pending = true;
      write_buffer.value = value;
      write_buffer.delay = apu_odd_cycle ? 4 : 3;
  }

  // Returns which events to trigger: bit 0 = quarter frame, bit 1 = half frame
  uint8_t clock() {
      // Process delayed writes
      if (unlikely(write_buffer.pending)) {
          if (write_buffer.delay > 0) {
              write_buffer.delay--;
          }
          // Check == 0 AFTER decrement so write takes effect on the
          // correct cycle (fixes off-by-one that made all events 1 cycle late).
          if (write_buffer.delay == 0 && write_buffer.pending) {
              uint8_t value = write_buffer.value;
              mode = (value >> 7) & 1;
              irq_inhibit = (value >> 6) & 1;

              if (irq_inhibit) {
                  irq_flag = false;
              }

              cycle = 0;
              write_buffer.pending = false;
              select_schedule();

              // 5-step mode: immediately clock quarter + half frame on write
              if (mode) {
                  return 3; // quarter + half frame
              }
              return 0;
          }
      }

      cycle++;

      // Fast path: not at the next scheduled event (~99.98% of ticks)
      if (likely(cycle != schedule_[event_index_].cycle)) return 0;

      // Process the scheduled event
      uint8_t flags = schedule_[event_index_].flags;
      uint8_t events = flags & (EVT_QF | EVT_HF);

      if (flags & EVT_IRQ) {
          if (!irq_inhibit) irq_flag = true;
      }

      if (flags & EVT_RESET) {
          cycle = 0;
          event_index_ = 0;
      } else {
          event_index_++;
      }

      return events;
  }
};

// ============================================================================
// Main APU
// ============================================================================
class APU : public SoundChipBase {
public:
  PulseChannel pulse1{true};
  PulseChannel pulse2{false};
  TriangleChannel triangle;
  NoiseChannel noise;
  DMCChannel dmc;
  FrameCounter frame;

private:
  bool is_pal = false;
  uint32_t cycle_counter = 0;

  // High-pass filter state (DC blocking, ~37 Hz cutoff for NTSC)
  mutable float hp_prev_in = 0.0f;
  mutable float hp_prev_out = 0.0f;

public:
  APU(bool pal = false) : is_pal(pal) {
    info_ = ChipInfo{pal ? "RP2A07-APU" : "RP2A03-APU", "Ricoh", pal ? "Ricoh 2A07 APU" : "Ricoh 2A03 APU"};
    init_regs(nes_apu::reg::REG_COUNT);
    noise.is_pal = pal;
    dmc.is_pal = pal;
    frame.is_pal = pal;
    reset_to_power_up_state();
#ifdef CERMU_HAS_CHIP_DEBUG
    wire_debug_registers(NES_APU_REG_INFO);
    debug_registry_.set_decl_entries(NES_APU_DECL_ENTRIES.data(), NES_APU_DECL_ENTRIES.size());
    register_debug_fields();
#endif
  }

  /// Power-on reset: disables all channels, writes $00 to $4017 with delay.
  void reset_to_power_up_state() {
    // All channels start disabled at power-on
    pulse1.length.reset();
    pulse2.length.reset();
    triangle.length.reset();
    noise.length.reset();
    pulse1.length.set_enabled(false);
    pulse2.length.set_enabled(false);
    triangle.length.set_enabled(false);
    noise.length.set_enabled(false);

    pulse1.sweep.reset();
    pulse2.sweep.reset();
    triangle.reset();

    // DMC starts silent with proper reset
    dmc.reset();

    // Noise LFSR properly initialized
    noise.shift_register = 1;

    // Reset filter state
    hp_prev_in = 0.0f;
    hp_prev_out = 0.0f;

    cycle_counter = 0;

    // Frame counter: full power-on reset.  The startup offset inside
    // power_on_reset() accounts for the hardware delay between the
    // implicit $4017=$00 write and the first user instruction.
    frame.power_on_reset();
  }

  /// Soft reset: re-triggers $4017 write, clears $4015.
  void reset_to_soft_state() {
    // Hardware reset internally writes $00 to $4015, disabling all
    // channels and zeroing their length counters.  Individual channel
    // register contents (duty, halt flags, envelopes, etc.) are
    // preserved — they can be re-activated by the reset handler.
    pulse1.length.set_enabled(false);
    pulse2.length.set_enabled(false);
    triangle.length.set_enabled(false);
    noise.length.set_enabled(false);

    // Triangle linear counter state is preserved across reset.

    pulse1.sweep.reset();
    pulse2.sweep.reset();

    // DMC: clear output, stop playback
    dmc.bytes_remaining = 0;
    dmc.irq_flag = false;

    // Noise LFSR preserved across reset on real hardware, but
    // re-seeding to 1 is harmless and avoids stuck-at-0 bugs.
    noise.shift_register = 1;

    // Reset filter state
    hp_prev_in = 0.0f;
    hp_prev_out = 0.0f;

    cycle_counter = 0;

    // Frame counter soft reset: preserves mode/inhibit, resets cycle.
    frame.soft_reset();
  }

  // MMIO Write Handler ($4000-$4017) with bus state support
  bus_state_t write(uint16_t addr, uint8_t value, bus_state_t bus_state) {
    switch (addr) {
    // Pulse 1
    case 0x4000:
      pulse1.write_control(value);
      break;
    case 0x4001:
      pulse1.write_sweep(value);
      break;
    case 0x4002:
      pulse1.write_timer_low(value);
      break;
    case 0x4003:
      pulse1.write_timer_high(value);
      break;

    // Pulse 2
    case 0x4004:
      pulse2.write_control(value);
      break;
    case 0x4005:
      pulse2.write_sweep(value);
      break;
    case 0x4006:
      pulse2.write_timer_low(value);
      break;
    case 0x4007:
      pulse2.write_timer_high(value);
      break;

    // Triangle
    case 0x4008:
      triangle.write_control(value);
      break;
    case 0x4009:
      break; // Unused
    case 0x400A:
      triangle.write_timer_low(value);
      break;
    case 0x400B:
      triangle.write_timer_high(value);
      break;

    // Noise
    case 0x400C:
      noise.write_control(value);
      break;
    case 0x400D:
      break; // Unused
    case 0x400E:
      noise.write_period(value);
      break;
    case 0x400F:
      noise.write_length(value);
      break;

    // DMC
    case 0x4010:
      dmc.write_control(value);
      break;
    case 0x4011:
      dmc.write_output(value);
      break;
    case 0x4012:
      dmc.write_address(value);
      break;
    case 0x4013:
      dmc.write_length(value);
      break;

    // Status
    case 0x4015:
      pulse1.length.set_enabled(value & 0x01);
      pulse2.length.set_enabled(value & 0x02);
      triangle.length.set_enabled(value & 0x04);
      noise.length.set_enabled(value & 0x08);

      if (value & 0x10) {
        if (!dmc.active()) {
          dmc.start();
        }
      } else {
        dmc.bytes_remaining = 0;
      }

      // Hardware-accurate: Writing to $4015 always clears DMC IRQ
      dmc.irq_flag = false;
      break;

    // Frame counter
    case 0x4017:
      frame.write(value, cycle_counter & 1);
      break;
    }

    // Update bus state with current data for open bus behavior
    BUS_SET_DATA(bus_state, value);
    return bus_state;
  }

  // MMIO Read Handler ($4015) with open bus behavior
  bus_state_t read(uint16_t addr, bus_state_t bus_state) {
    if (addr == 0x4015) {
      uint8_t status = 0;

      status |= (pulse1.length.active() ? 0x01 : 0);
      status |= (pulse2.length.active() ? 0x02 : 0);
      status |= (triangle.length.active() ? 0x04 : 0);
      status |= (noise.length.active() ? 0x08 : 0);
      status |= (dmc.active() ? 0x10 : 0);
      status |= (frame.irq_flag ? 0x40 : 0);
      status |= (dmc.irq_flag ? 0x80 : 0);

      // Reading $4015 clears frame IRQ flag
      frame.irq_flag = false;

      BUS_SET_DATA(bus_state, status);
    }
    // Other addresses return open bus (previous data on bus)

    return bus_state;
  }

  // Main APU tick - called every CPU cycle
  bus_state_t tick(bus_state_t bus_state) {
    // Frame counter events
    uint8_t events = frame.clock();
    bool had_half_frame = false;

    if (events & 1) { // Quarter frame
      pulse1.envelope.clock();
      pulse2.envelope.clock();
      triangle.clock_linear_counter();
      noise.envelope.clock();
    }

    if (events & 2) { // Half frame
      had_half_frame = true;

      pulse1.length.clock();
      pulse2.length.clock();
      triangle.length.clock();
      noise.length.clock();

      pulse1.sweep.clock(pulse1.timer_period);
      pulse2.sweep.clock(pulse2.timer_period);
    }

    // Resolve any pending length reloads.  On half-frame cycles, a
    // reload is dropped if the post-clock counter is still > 0.
    // On other cycles, reloads always succeed.
    pulse1.length.resolve_pending_reload(had_half_frame);
    pulse2.length.resolve_pending_reload(had_half_frame);
    triangle.length.resolve_pending_reload(had_half_frame);
    noise.length.resolve_pending_reload(had_half_frame);

    // Triangle clocks every CPU cycle
    triangle.clock();

    // DMC clocks every CPU cycle
    dmc.clock();

    // Pulse and Noise clock at half CPU rate (APU cycle = every other CPU cycle)
    if (cycle_counter & 1) {
      pulse1.clock();
      pulse2.clock();
      noise.clock();
    }

    cycle_counter++;

    // Latch halt flags for next cycle.  The length counter clock uses
    // prev_halt_ so that writes to halt on the SAME cycle as a half-frame
    // event take effect AFTER the clock (real hardware behavior).
    pulse1.length.update_prev_halt();
    pulse2.length.update_prev_halt();
    triangle.length.update_prev_halt();
    noise.length.update_prev_halt();

    return bus_state;
  }

  /// MMIO-only tick for multi-threaded audio mode.
  ///
  /// Runs on the emulation thread to maintain IRQ, DMA, and $4015 state.
  /// Audio signal generation (envelopes, oscillators, sweep) is skipped —
  /// those run on the audio thread via a second APU instance.
  ///
  /// In single-threaded mode, use tick() instead which does everything.
  void tick_mmio() {
    uint8_t events = frame.clock();
    bool had_half_frame = false;

    // Quarter frame: envelopes and linear counter are audio-only — skip

    if (events & 2) { // Half frame
      had_half_frame = true;
      // Length counters affect $4015 readback — must track on emu thread
      pulse1.length.clock();
      pulse2.length.clock();
      triangle.length.clock();
      noise.length.clock();
      // Sweep is audio-only — skip
    }

    // Resolve pending length reloads (affects $4015 active bits)
    pulse1.length.resolve_pending_reload(had_half_frame);
    pulse2.length.resolve_pending_reload(had_half_frame);
    triangle.length.resolve_pending_reload(had_half_frame);
    noise.length.resolve_pending_reload(had_half_frame);

    // DMC timing: drives needs_sample (DMA) and irq_flag.
    // The output unit state (output_level, shift_register) is also
    // updated but not read by any MMIO query — harmless dead weight.
    dmc.clock();

    // Oscillator timers (pulse, triangle, noise) are audio-only — skip

    cycle_counter++;

    pulse1.length.update_prev_halt();
    pulse2.length.update_prev_halt();
    triangle.length.update_prev_halt();
    noise.length.update_prev_halt();
  }

  /// Simplified register write for command-queue use (audio thread).
  /// @param reg  Register offset 0x00–0x17 (relative to $4000).
  /// @param value  Data byte.
  void write_register(uint8_t reg, uint8_t value) {
    write(0x4000 | reg, value, 0);
  }

  // Generate audio sample using precomputed NES mixer lookup tables
  // Reference: https://www.nesdev.org/wiki/APU_Mixer
  float sample() const {
    uint8_t p1 = pulse1.output();
    uint8_t p2 = pulse2.output();
    uint8_t tri = triangle.output();
    uint8_t noi = noise.output();
    uint8_t dm = dmc.output();

    // Lookup-table mixing: pulse_table[p1+p2] + tnd_table[3*tri + 2*noi + dmc]
    float output = apu_mixer::pulse_table[p1 + p2]
                 + apu_mixer::tnd_table[3 * tri + 2 * noi + dm];

    // Simple first-order high-pass filter for DC removal (~37 Hz at 1.789 MHz)
    // y[n] = x[n] - x[n-1] + R * y[n-1], R ≈ 0.9996
    float filtered = output - hp_prev_in + 0.9996f * hp_prev_out;
    hp_prev_in = output;
    hp_prev_out = filtered;

    return filtered;
  }

  // Helper: Get DMC sample request for DMA
  bool dmc_needs_sample() const { return dmc.needs_sample; }

  uint16_t dmc_sample_address() const { return dmc.current_address; }

  void dmc_load_sample(uint8_t data) { dmc.load_sample(data); }

  // Helper: Check for IRQ
  bool irq() const { return frame.irq_flag || dmc.irq_flag; }

  // --- ChipBase interface ---
#ifdef CERMU_HAS_GUI
  bool has_settings_content() const override;
  void render_settings_content() override;
  ChipLayout* create_chip_layout() const override;
  std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
  void register_debug_fields() {
    using AP = const APU;
    auto& r = debug_registry_;

    // ---- Status ($4015) ----
    r.category("Status ($4015)");
    r.flag("Pulse 1 Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.length.active(); });
    r.flag("Pulse 2 Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.length.active(); });
    r.flag("Triangle Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->triangle.length.active(); });
    r.flag("Noise Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.length.active(); });
    r.flag("DMC Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.active(); });
    r.flag("Frame IRQ", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->frame.irq_flag; });
    r.flag("DMC IRQ", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.irq_flag; });

    // ---- Pulse 1 ($4000-$4003) ----
    {
        static constexpr const char* duty_names[] = {"12.5%", "25%", "50%", "75%"};
        r.category("Pulse 1 ($4000-$4003)");
        r.value("Output", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.output(); }, 4);
        r.state("Duty", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.duty; }, duty_names, 4);
        r.value("Timer Period", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.timer_period; }, 11);
        r.flag("Length Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.length.active(); });
        r.counter("Length Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.length.value(); }, 255);
        // Envelope
        r.flag("Env Constant", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.envelope.constant_volume; });
        r.value("Env Volume", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.envelope.volume(); }, 4);
        r.flag("Env Loop", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.envelope.loop; });
        // Sweep
        r.flag("Sweep Enable", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.sweep.enabled; });
        r.value("Sweep Period", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.sweep.period; }, 3);
        r.flag("Sweep Negate", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.sweep.negate; });
        r.value("Sweep Shift", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse1.sweep.shift; }, 3);
        r.flag("Sweep Muting", +[](const ChipBase* c) -> uint32_t {
            return static_cast<AP*>(c)->pulse1.sweep.is_muting(static_cast<AP*>(c)->pulse1.timer_period);
        });
        r.value("Sweep Target", +[](const ChipBase* c) -> uint32_t {
            return static_cast<uint32_t>(static_cast<AP*>(c)->pulse1.sweep.calculate_target(static_cast<AP*>(c)->pulse1.timer_period));
        }, 11);
    }

    // ---- Pulse 2 ($4004-$4007) ----
    {
        static constexpr const char* duty_names[] = {"12.5%", "25%", "50%", "75%"};
        r.category("Pulse 2 ($4004-$4007)");
        r.value("Output", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.output(); }, 4);
        r.state("Duty", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.duty; }, duty_names, 4);
        r.value("Timer Period", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.timer_period; }, 11);
        r.flag("Length Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.length.active(); });
        r.counter("Length Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.length.value(); }, 255);
        // Envelope
        r.flag("Env Constant", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.envelope.constant_volume; });
        r.value("Env Volume", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.envelope.volume(); }, 4);
        r.flag("Env Loop", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.envelope.loop; });
        // Sweep
        r.flag("Sweep Enable", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.sweep.enabled; });
        r.value("Sweep Period", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.sweep.period; }, 3);
        r.flag("Sweep Negate", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.sweep.negate; });
        r.value("Sweep Shift", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->pulse2.sweep.shift; }, 3);
        r.flag("Sweep Muting", +[](const ChipBase* c) -> uint32_t {
            return static_cast<AP*>(c)->pulse2.sweep.is_muting(static_cast<AP*>(c)->pulse2.timer_period);
        });
        r.value("Sweep Target", +[](const ChipBase* c) -> uint32_t {
            return static_cast<uint32_t>(static_cast<AP*>(c)->pulse2.sweep.calculate_target(static_cast<AP*>(c)->pulse2.timer_period));
        }, 11);
    }

    // ---- Triangle ($4008-$400B) ----
    r.category("Triangle ($4008-$400B)");
    r.value("Output", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->triangle.output(); }, 4);
    r.value("Timer Period", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->triangle.timer_period; }, 11);
    r.flag("Length Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->triangle.length.active(); });
    r.counter("Length Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->triangle.length.value(); }, 255);
    r.value("Linear Counter Load", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->triangle.linear_counter_load; }, 7);
    r.flag("Control Flag", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->triangle.control_flag; });

    // ---- Noise ($400C-$400F) ----
    r.category("Noise ($400C-$400F)");
    r.value("Output", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.output(); }, 4);
    static constexpr const char* noise_mode_names[] = {"15-bit", "6-bit"};
    r.state("Mode", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.mode ? 1u : 0u; },
        noise_mode_names, 2);
    r.value("Period Index", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.period_index; }, 4);
    r.flag("Length Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.length.active(); });
    r.counter("Length Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.length.value(); }, 255);
    r.flag("Env Constant", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.envelope.constant_volume; });
    r.value("Env Volume", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.envelope.volume(); }, 4);
    r.flag("Env Loop", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.envelope.loop; });
    r.value("Shift Register", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->noise.shift_register; }, 16);

    // ---- DMC ($4010-$4013) ----
    r.category("DMC ($4010-$4013)");
    r.flag("IRQ Enable", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.irq_enabled; });
    r.flag("Loop", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.loop; });
    r.value("Rate Index", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.rate_index; }, 4);
    r.counter("Output Level", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.output_level; }, 127);
    r.address("Sample Address", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.sample_address; }, 16);
    r.value("Sample Length", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.sample_length; }, 16);
    r.address("Current Address", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.current_address; }, 16);
    r.value("Bytes Remaining", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.bytes_remaining; }, 16);
    r.flag("Active", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.active(); });
    r.flag("IRQ Flag", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.irq_flag; });
    r.flag("Needs Sample", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->dmc.needs_sample; });

    // ---- Frame Counter ($4017) ----
    r.category("Frame Counter ($4017)");
    static constexpr const char* frame_mode_names[] = {"4-step", "5-step"};
    r.state("Mode", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->frame.mode ? 1u : 0u; },
        frame_mode_names, 2);
    r.flag("IRQ Inhibit", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->frame.irq_inhibit; });
    r.flag("IRQ Flag", +[](const ChipBase* c) -> uint32_t { return static_cast<AP*>(c)->frame.irq_flag; });

    // ---- Mixer ----
    r.category("Mixer Output", false);
    r.level("Mixed Output", +[](const ChipBase* c) -> float {
        float s = static_cast<AP*>(c)->sample();
        return s < 0.0f ? -s : s;
    });
  }
#endif
};

} // namespace nes6502_apu
