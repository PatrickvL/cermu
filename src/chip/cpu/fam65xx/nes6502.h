#pragma once
/*
 * nes6502.h - NES APU (Audio Processing Unit) Implementation
 *
 * Contains the APU class used by the NES 6502 CPU template (RICOH_2A03Traits).
 * The APU is integrated into the CPU via the apu_mixin in fam65xx_mixins.hpp.
 * The CPU type itself is fam65xx::RICOH_2A03 defined in fam65xx.hpp.
 */

#include <array>
#include <cmath>
#include <cstring>

#include <cstdint>

#include "../../../core/chip.h"
#include "../../../core/system_lines.h"
#include "fam65xx_types.h"

// ============================================================================
// INTEGRATED APU IMPLEMENTATION (C++)
// ============================================================================

// APU Constants
constexpr uint32_t CPU_FREQ_NTSC = 1789773;
constexpr uint32_t CPU_FREQ_PAL = 1662607;

// Length counter lookup table
constexpr uint8_t APU_LENGTH_TABLE[32] = {
    10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60,  10, 14, 12, 26, 14,
    12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72,  26, 16, 28, 32, 30};

// Noise period tables
constexpr uint16_t NOISE_PERIOD_NTSC[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};

constexpr uint16_t NOISE_PERIOD_PAL[16] = {
    4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778};

// DMC period tables
constexpr uint16_t DMC_PERIOD_NTSC[16] = {428, 380, 340, 320, 286, 254,
                                          226, 214, 190, 160, 142, 128,
                                          106, 84,  72,  54};

constexpr uint16_t DMC_PERIOD_PAL[16] = {398, 354, 316, 298, 276, 236, 210, 198,
                                         176, 148, 132, 118, 98,  78,  66,  50};

// Duty cycle sequences (8 steps each)
constexpr uint8_t DUTY_TABLE[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0}, // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0}, // 25%
    {0, 1, 1, 1, 1, 0, 0, 0}, // 50%
    {1, 0, 0, 1, 1, 1, 1, 1}  // 25% negated
};

// Triangle sequence (32 steps)
constexpr uint8_t TRIANGLE_TABLE[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,  4,  3,  2,  1,  0,
    0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

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

public:
  void load(uint8_t index) {
    if (enabled) {
      counter = APU_LENGTH_TABLE[index];
    }
  }

  void clock() {
    if (!halt && counter > 0) {
      counter--;
    }
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

  /// Legacy alias — defaults to power-on reset for backward compat.
  void reset() { power_on_reset(); }

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
class APU : public ChipBase {
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
    info_ = ChipInfo{pal ? "RP2A07-APU" : "RP2A03-APU", "Ricoh"};
    noise.is_pal = pal;
    dmc.is_pal = pal;
    frame.is_pal = pal;
    reset_to_power_up_state();
  }

  /// Power-on reset: disables all channels, writes $00 to $4017 with delay.
  void reset_to_power_up_state() {
    // All channels start disabled at power-on
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
    FAM65XX_SET_DATA(bus_state, value);
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

      FAM65XX_SET_DATA(bus_state, status);
    }
    // Other addresses return open bus (previous data on bus)

    return bus_state;
  }

  // Main APU tick - called every CPU cycle
  bus_state_t tick(bus_state_t bus_state) {
    // Frame counter events
    uint8_t events = frame.clock();

    if (events & 1) { // Quarter frame
      pulse1.envelope.clock();
      pulse2.envelope.clock();
      triangle.clock_linear_counter();
      noise.envelope.clock();
    }

    if (events & 2) { // Half frame
      pulse1.length.clock();
      pulse2.length.clock();
      triangle.length.clock();
      noise.length.clock();

      pulse1.sweep.clock(pulse1.timer_period);
      pulse2.sweep.clock(pulse2.timer_period);
    }

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

    return bus_state;
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
  bool has_debug_content()    const override;
  bool has_settings_content() const override;
  bool has_layout_content()   const override;
  void render_debug_content()    override;
  void render_settings_content() override;
  void render_layout_content()   override;
};

} // namespace nes6502_apu

// ============================================================================
// Convenience re-exports — include this header to get the NES CPU type
// without pulling in fam65xx.hpp directly.
// NOTE: fam65xx.hpp must be included separately since this header is
// also included by fam65xx_mixins.hpp (avoid circular dependency).
// ============================================================================
// Usage:  #include "nes6502.h"
//         #include "fam65xx.hpp"   // provides fam65xx::RICOH_2A03
// Or include fam65xx.hpp first and this header for APU only.
