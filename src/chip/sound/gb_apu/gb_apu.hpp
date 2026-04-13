#pragma once
/*
 * gb_apu.hpp — Game Boy APU (Audio Processing Unit)
 *
 * The Game Boy APU provides 4 audio channels:
 *   Channel 1: Square wave with sweep (frequency sweep, volume envelope, duty cycle)
 *   Channel 2: Square wave (volume envelope, duty cycle)
 *   Channel 3: Programmable wave (32 4-bit samples)
 *   Channel 4: Noise (LFSR-based, volume envelope)
 *
 * Global: master volume, channel panning (L/R), power control.
 *
 * Frame sequencer: 512 Hz (CPU_FREQ / 8192), clocks length, envelope, and sweep.
 *   Step 0: Length
 *   Step 1: —
 *   Step 2: Length, Sweep
 *   Step 3: —
 *   Step 4: Length
 *   Step 5: —
 *   Step 6: Length, Sweep
 *   Step 7: Volume Envelope
 *
 * Register map ($FF10–$FF3F):
 *   $FF10–$FF14: Channel 1 (sweep, duty, volume, frequency)
 *   $FF15–$FF19: Channel 2 (duty, volume, frequency) [$FF15 unused]
 *   $FF1A–$FF1E: Channel 3 (enable, length, output level, frequency)
 *   $FF1F–$FF23: Channel 4 (length, volume, polynomial, trigger)
 *   $FF24: NR50 — Master volume + VIN enable
 *   $FF25: NR51 — Channel panning (L/R per channel)
 *   $FF26: NR52 — Sound on/off + channel status
 *   $FF30–$FF3F: Wave RAM (16 bytes = 32 4-bit samples)
 */

#include "chip/sound/sound_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>
#include <cstring>

#define GB_APU_DECL(REG, FLD, CMP) \
    /* Channel 1 */ \
    REG(0x00, NR10,  "Ch1 Sweep")                                              \
      FLD(NR10, SWEEP_TIME,  6:4, "Sweep time",        Value, 0, 0)           \
      FLD(NR10, SWEEP_DIR,   3:3, "Sweep direction",   Flag,  0, 0)           \
      FLD(NR10, SWEEP_SHIFT, 2:0, "Sweep shift",       Value, 0, 0)           \
    REG(0x01, NR11,  "Ch1 Duty/Length")                                        \
      FLD(NR11, DUTY,        7:6, "Duty cycle",        Value, 0, 0)           \
      FLD(NR11, LENGTH,      5:0, "Length load",        Value, 0, 0)           \
    REG(0x02, NR12,  "Ch1 Volume Envelope")                                    \
      FLD(NR12, INIT_VOL,    7:4, "Initial volume",    Value, 0, 0)           \
      FLD(NR12, ENV_DIR,     3:3, "Envelope direction", Flag,  0, 0)          \
      FLD(NR12, ENV_PERIOD,  2:0, "Envelope period",   Value, 0, 0)           \
    REG(0x03, NR13,  "Ch1 Frequency low")                                      \
    REG(0x04, NR14,  "Ch1 Frequency high / Trigger")                           \
      FLD(NR14, TRIGGER,     7:7, "Trigger",           Flag,  0, 0)           \
      FLD(NR14, LENGTH_EN,   6:6, "Length enable",      Flag,  0, 0)          \
      FLD(NR14, FREQ_HI,     2:0, "Frequency high bits",Value, 0, 0)         \
    /* Channel 2 */ \
    REG(0x05, NR20,  "Ch2 (unused)")                                           \
    REG(0x06, NR21,  "Ch2 Duty/Length")                                        \
    REG(0x07, NR22,  "Ch2 Volume Envelope")                                    \
    REG(0x08, NR23,  "Ch2 Frequency low")                                      \
    REG(0x09, NR24,  "Ch2 Frequency high / Trigger")                           \
    /* Channel 3 */ \
    REG(0x0A, NR30,  "Ch3 Enable")                                             \
      FLD(NR30, DAC_EN,      7:7, "DAC enable",        Flag,  0, 0)           \
    REG(0x0B, NR31,  "Ch3 Length")                                             \
    REG(0x0C, NR32,  "Ch3 Output Level")                                       \
      FLD(NR32, OUT_LEVEL,   6:5, "Output level",      Value, 0, 0)           \
    REG(0x0D, NR33,  "Ch3 Frequency low")                                      \
    REG(0x0E, NR34,  "Ch3 Frequency high / Trigger")                           \
    /* Channel 4 */ \
    REG(0x0F, NR40,  "Ch4 (unused)")                                           \
    REG(0x10, NR41,  "Ch4 Length")                                             \
    REG(0x11, NR42,  "Ch4 Volume Envelope")                                    \
    REG(0x12, NR43,  "Ch4 Polynomial Counter")                                 \
      FLD(NR43, SHIFT_CLK,   7:4, "Shift clock freq",  Value, 0, 0)           \
      FLD(NR43, WIDTH_MODE,  3:3, "Counter width",     Flag,  0, 0)           \
      FLD(NR43, DIV_RATIO,   2:0, "Dividing ratio",    Value, 0, 0)           \
    REG(0x13, NR44,  "Ch4 Trigger")                                            \
    /* Master control */ \
    REG(0x14, NR50,  "Master Volume")                                          \
    REG(0x15, NR51,  "Channel Panning")                                        \
    REG(0x16, NR52,  "Sound On/Off")                                           \
      FLD(NR52, POWER,       7:7, "All sound on/off",  Flag,  0, 0)

namespace gb_apu {
    namespace reg {
        GB_APU_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 23;
    }
    using namespace reg;

    inline constexpr int WAVE_RAM_SIZE = 16;  // 32 × 4-bit samples
    inline constexpr uint32_t CPU_FREQ = 4194304;  // 4.194304 MHz

    // Frame sequencer period: CPU_FREQ / 512 = 8192 T-states
    inline constexpr uint32_t FRAME_SEQ_PERIOD = 8192;

    // Duty cycle waveforms (8 steps each)
    // Index: duty_cycle (0-3), phase (0-7) → 0 or 1
    inline constexpr uint8_t DUTY_TABLE[4] = {
        0b00000001,  // 12.5%
        0b10000001,  // 25%
        0b10000111,  // 50%
        0b01111110,  // 75%
    };
}

DECL_EXTRACT(GB_APU, GB_APU_DECL)

// ============================================================================
// Game Boy APU — Full Implementation
// ============================================================================

struct gb_apu_t : public SoundChipBase {

    gb_apu_t()
        : SoundChipBase(ChipInfo{"SM83_APU", "Sharp", "Game Boy Audio Processing Unit"})
    {
        init_regs(gb_apu::reg::REG_COUNT);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(GB_APU_REG_INFO);
        register_debug_fields();
#endif
    }

    bool has_mmio() const override { return true; }

    // ── Read mask table: many APU registers have bits that read back as 1 ──
    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x3F;
        if (addr < gb_apu::reg::REG_COUNT) {
            // NR52 returns power bit + channel active flags
            if (addr == gb_apu::NR52) {
                uint8_t nr52 = 0x70;  // Bits 4-6 always 1
                if (power_on_) nr52 |= 0x80;
                if (ch1_.enabled) nr52 |= 0x01;
                if (ch2_.enabled) nr52 |= 0x02;
                if (ch3_.enabled) nr52 |= 0x04;
                if (ch4_.enabled) nr52 |= 0x08;
                BUS_SET_DATA(bus, nr52);
            } else {
                BUS_SET_DATA(bus, regs_.data[addr]);
            }
        } else if (addr >= 0x20 && addr < 0x30) {
            // Wave RAM: $FF30–$FF3F
            BUS_SET_DATA(bus, wave_ram_[addr - 0x20]);
        } else {
            BUS_SET_DATA(bus, 0xFF);
        }
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t addr = BUS_GET_ADDR(bus) & 0x3F;
        uint8_t data = BUS_GET_DATA(bus);

        if (addr >= 0x20 && addr < 0x30) {
            // Wave RAM: writable any time
            wave_ram_[addr - 0x20] = data;
            return bus;
        }

        if (addr >= gb_apu::reg::REG_COUNT) return bus;

        // NR52: only bit 7 (power) is writable
        if (addr == gb_apu::NR52) {
            bool new_power = (data & 0x80) != 0;
            if (!new_power && power_on_) {
                // Power off: clear all registers
                for (int i = 0; i < gb_apu::NR52; i++)
                    regs_.data[i] = 0;
                ch1_ = {}; ch2_ = {}; ch3_ = {}; ch4_ = {};
            }
            power_on_ = new_power;
            return bus;
        }

        // Writes ignored when powered off (except NR52)
        if (!power_on_) return bus;

        regs_.data[addr] = data;

        // Handle trigger and channel-specific register writes
        switch (addr) {
            case gb_apu::NR14: if (data & 0x80) trigger_ch1(); break;
            case gb_apu::NR24: if (data & 0x80) trigger_ch2(); break;
            case gb_apu::NR34: if (data & 0x80) trigger_ch3(); break;
            case gb_apu::NR44: if (data & 0x80) trigger_ch4(); break;
            default: break;
        }

        return bus;
    }

    // ── Per T-state tick — generates one audio sample via decimation ──
    float tick() noexcept {
        if (!power_on_) return 0.0f;

        // Frame sequencer: 512 Hz (8192 T-states per step)
        frame_seq_counter_++;
        if (frame_seq_counter_ >= gb_apu::FRAME_SEQ_PERIOD) {
            frame_seq_counter_ = 0;
            clock_frame_sequencer();
        }

        // Channel frequency timers — produce waveform output
        float ch1_out = tick_square(ch1_, true);
        float ch2_out = tick_square(ch2_, false);
        float ch3_out = tick_wave();
        float ch4_out = tick_noise();

        // Mixer: combine channels with master volume and panning
        uint8_t nr50 = regs_.data[gb_apu::NR50];
        uint8_t nr51 = regs_.data[gb_apu::NR51];

        float left = 0.0f, right = 0.0f;
        if (nr51 & 0x10) left += ch1_out;
        if (nr51 & 0x01) right += ch1_out;
        if (nr51 & 0x20) left += ch2_out;
        if (nr51 & 0x02) right += ch2_out;
        if (nr51 & 0x40) left += ch3_out;
        if (nr51 & 0x04) right += ch3_out;
        if (nr51 & 0x80) left += ch4_out;
        if (nr51 & 0x08) right += ch4_out;

        int left_vol  = ((nr50 >> 4) & 0x07) + 1;
        int right_vol = (nr50 & 0x07) + 1;
        left  *= static_cast<float>(left_vol)  / 32.0f;
        right *= static_cast<float>(right_vol) / 32.0f;

        // Return mono mix for single audio port
        return (left + right) * 0.5f;
    }

    void reset() override {
        std::memset(regs_.data, 0, gb_apu::reg::REG_COUNT);
        std::memset(wave_ram_, 0, sizeof(wave_ram_));
        power_on_ = false;
        frame_seq_counter_ = 0;
        frame_seq_step_ = 0;
        ch1_ = {}; ch2_ = {}; ch3_ = {}; ch4_ = {};
    }

    uint8_t wave_ram_[gb_apu::WAVE_RAM_SIZE] = {};

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    bool power_on_ = false;
    uint32_t frame_seq_counter_ = 0;
    uint8_t  frame_seq_step_ = 0;

    // ── Square channel state (Ch1 and Ch2) ───────────────────────────
    struct SquareChannel {
        bool     enabled = false;
        uint16_t freq_timer = 0;     // Frequency countdown timer
        uint16_t freq_period = 0;    // Period reload value (2048 - frequency) * 4
        uint8_t  duty = 0;           // Duty cycle (0-3)
        uint8_t  duty_pos = 0;       // Position in 8-step duty cycle
        uint8_t  volume = 0;         // Current volume (0-15)
        uint8_t  env_period = 0;     // Envelope period
        uint8_t  env_timer = 0;      // Envelope countdown
        bool     env_add = false;    // Envelope direction (true = increase)
        uint16_t length_counter = 0; // Length counter (counts down)
        bool     length_en = false;  // Length counter enable
        bool     dac_on = false;     // DAC on/off (volume envelope init != 0 or env dir up)
        // Sweep (Ch1 only)
        uint16_t shadow_freq = 0;
        uint8_t  sweep_period = 0;
        uint8_t  sweep_timer = 0;
        uint8_t  sweep_shift = 0;
        bool     sweep_negate = false;
        bool     sweep_enabled = false;
    };

    // ── Wave channel state (Ch3) ─────────────────────────────────────
    struct WaveChannel {
        bool     enabled = false;
        uint16_t freq_timer = 0;
        uint16_t freq_period = 0;
        uint8_t  sample_pos = 0;     // Position in 32-sample wave (0-31)
        uint8_t  output_level = 0;   // Volume shift (0=mute, 1=100%, 2=50%, 3=25%)
        uint16_t length_counter = 0;
        bool     length_en = false;
        bool     dac_on = false;
    };

    // ── Noise channel state (Ch4) ────────────────────────────────────
    struct NoiseChannel {
        bool     enabled = false;
        uint32_t freq_timer = 0;
        uint32_t freq_period = 0;
        uint16_t lfsr = 0x7FFF;      // 15-bit linear feedback shift register
        bool     width_mode = false;  // true = 7-bit mode, false = 15-bit
        uint8_t  volume = 0;
        uint8_t  env_period = 0;
        uint8_t  env_timer = 0;
        bool     env_add = false;
        uint16_t length_counter = 0;
        bool     length_en = false;
        bool     dac_on = false;
    };

    SquareChannel ch1_, ch2_;
    WaveChannel   ch3_;
    NoiseChannel  ch4_;

    // ── Frame sequencer (512 Hz) ─────────────────────────────────────
    void clock_frame_sequencer() noexcept {
        switch (frame_seq_step_) {
            case 0: clock_length_all(); break;
            case 1: break;
            case 2: clock_length_all(); clock_sweep(); break;
            case 3: break;
            case 4: clock_length_all(); break;
            case 5: break;
            case 6: clock_length_all(); clock_sweep(); break;
            case 7: clock_envelope_all(); break;
        }
        frame_seq_step_ = (frame_seq_step_ + 1) & 0x07;
    }

    // ── Length counter ────────────────────────────────────────────────
    void clock_length(bool& enabled, uint16_t& counter, bool length_en) noexcept {
        if (length_en && counter > 0) {
            counter--;
            if (counter == 0) enabled = false;
        }
    }

    void clock_length_all() noexcept {
        clock_length(ch1_.enabled, ch1_.length_counter, ch1_.length_en);
        clock_length(ch2_.enabled, ch2_.length_counter, ch2_.length_en);
        clock_length(ch3_.enabled, ch3_.length_counter, ch3_.length_en);
        clock_length(ch4_.enabled, ch4_.length_counter, ch4_.length_en);
    }

    // ── Volume envelope ──────────────────────────────────────────────
    void clock_envelope(uint8_t& volume, uint8_t& timer, uint8_t period, bool add) noexcept {
        if (period == 0) return;
        if (timer > 0) timer--;
        if (timer == 0) {
            timer = period;
            if (add && volume < 15) volume++;
            else if (!add && volume > 0) volume--;
        }
    }

    void clock_envelope_all() noexcept {
        clock_envelope(ch1_.volume, ch1_.env_timer, ch1_.env_period, ch1_.env_add);
        clock_envelope(ch2_.volume, ch2_.env_timer, ch2_.env_period, ch2_.env_add);
        clock_envelope(ch4_.volume, ch4_.env_timer, ch4_.env_period, ch4_.env_add);
    }

    // ── Frequency sweep (Ch1 only) ───────────────────────────────────
    void clock_sweep() noexcept {
        if (!ch1_.sweep_enabled || ch1_.sweep_period == 0) return;
        if (ch1_.sweep_timer > 0) ch1_.sweep_timer--;
        if (ch1_.sweep_timer == 0) {
            ch1_.sweep_timer = ch1_.sweep_period ? ch1_.sweep_period : 8;
            if (ch1_.sweep_period > 0) {
                uint16_t new_freq = calc_sweep_freq();
                if (new_freq <= 2047 && ch1_.sweep_shift > 0) {
                    ch1_.shadow_freq = new_freq;
                    ch1_.freq_period = (2048 - new_freq) * 4;
                    // Write back to frequency registers
                    regs_.data[gb_apu::NR13] = static_cast<uint8_t>(new_freq & 0xFF);
                    regs_.data[gb_apu::NR14] = (regs_.data[gb_apu::NR14] & 0xF8) |
                                               static_cast<uint8_t>((new_freq >> 8) & 0x07);
                    // Overflow check with new frequency
                    if (calc_sweep_freq() > 2047) ch1_.enabled = false;
                } else if (new_freq > 2047) {
                    ch1_.enabled = false;
                }
            }
        }
    }

    uint16_t calc_sweep_freq() const noexcept {
        uint16_t delta = ch1_.shadow_freq >> ch1_.sweep_shift;
        return ch1_.sweep_negate ? (ch1_.shadow_freq - delta) : (ch1_.shadow_freq + delta);
    }

    // ── Square wave tick (Ch1/Ch2) ───────────────────────────────────
    float tick_square(SquareChannel& ch, bool /*is_ch1*/) noexcept {
        if (!ch.enabled || !ch.dac_on) return 0.0f;

        if (ch.freq_timer > 0) ch.freq_timer--;
        if (ch.freq_timer == 0) {
            ch.freq_timer = ch.freq_period;
            ch.duty_pos = (ch.duty_pos + 1) & 0x07;
        }

        // Duty waveform: 1-bit from duty table
        uint8_t duty_bit = (gb_apu::DUTY_TABLE[ch.duty] >> ch.duty_pos) & 1;

        // Volume scale: 0-15 → 0.0-1.0
        float sample = duty_bit ? (static_cast<float>(ch.volume) / 15.0f) : 0.0f;
        return sample;
    }

    // ── Wave channel tick (Ch3) ──────────────────────────────────────
    float tick_wave() noexcept {
        if (!ch3_.enabled || !ch3_.dac_on) return 0.0f;

        if (ch3_.freq_timer > 0) ch3_.freq_timer--;
        if (ch3_.freq_timer == 0) {
            ch3_.freq_timer = ch3_.freq_period;
            ch3_.sample_pos = (ch3_.sample_pos + 1) & 0x1F;
        }

        // Read 4-bit sample from wave RAM
        uint8_t byte = wave_ram_[ch3_.sample_pos >> 1];
        uint8_t sample = (ch3_.sample_pos & 1) ? (byte & 0x0F) : (byte >> 4);

        // Output level shift: 0=mute, 1=100%, 2=50%, 3=25%
        if (ch3_.output_level == 0) return 0.0f;
        sample >>= (ch3_.output_level - 1);

        return static_cast<float>(sample) / 15.0f;
    }

    // ── Noise channel tick (Ch4) ─────────────────────────────────────
    float tick_noise() noexcept {
        if (!ch4_.enabled || !ch4_.dac_on) return 0.0f;

        if (ch4_.freq_timer > 0) ch4_.freq_timer--;
        if (ch4_.freq_timer == 0) {
            ch4_.freq_timer = ch4_.freq_period;

            // LFSR clock: XOR bits 0 and 1, shift right, new bit goes to bit 14
            uint8_t xor_bit = (ch4_.lfsr & 0x01) ^ ((ch4_.lfsr >> 1) & 0x01);
            ch4_.lfsr >>= 1;
            ch4_.lfsr |= (xor_bit << 14);
            if (ch4_.width_mode) {
                // 7-bit mode: also set bit 6
                ch4_.lfsr &= ~(1 << 6);
                ch4_.lfsr |= (xor_bit << 6);
            }
        }

        // Output: inverted bit 0 of LFSR
        uint8_t output = (~ch4_.lfsr) & 0x01;
        return output ? (static_cast<float>(ch4_.volume) / 15.0f) : 0.0f;
    }

    // ── Channel triggers ─────────────────────────────────────────────
    void trigger_ch1() noexcept {
        uint8_t nr10 = regs_.data[gb_apu::NR10];
        uint8_t nr11 = regs_.data[gb_apu::NR11];
        uint8_t nr12 = regs_.data[gb_apu::NR12];
        uint16_t freq = regs_.data[gb_apu::NR13] |
                        ((regs_.data[gb_apu::NR14] & 0x07) << 8);

        ch1_.dac_on = (nr12 & 0xF8) != 0;
        ch1_.enabled = ch1_.dac_on;
        ch1_.duty = (nr11 >> 6) & 0x03;
        ch1_.length_counter = ch1_.length_counter ? ch1_.length_counter : (64 - (nr11 & 0x3F));
        ch1_.length_en = (regs_.data[gb_apu::NR14] & 0x40) != 0;
        ch1_.volume = (nr12 >> 4) & 0x0F;
        ch1_.env_period = nr12 & 0x07;
        ch1_.env_timer = ch1_.env_period;
        ch1_.env_add = (nr12 & 0x08) != 0;
        ch1_.freq_period = (2048 - freq) * 4;
        ch1_.freq_timer = ch1_.freq_period;

        // Sweep setup
        ch1_.sweep_period = (nr10 >> 4) & 0x07;
        ch1_.sweep_shift = nr10 & 0x07;
        ch1_.sweep_negate = (nr10 & 0x08) != 0;
        ch1_.shadow_freq = freq;
        ch1_.sweep_timer = ch1_.sweep_period ? ch1_.sweep_period : 8;
        ch1_.sweep_enabled = (ch1_.sweep_period > 0 || ch1_.sweep_shift > 0);

        // Overflow check on trigger
        if (ch1_.sweep_shift > 0 && calc_sweep_freq() > 2047)
            ch1_.enabled = false;
    }

    void trigger_ch2() noexcept {
        uint8_t nr21 = regs_.data[gb_apu::NR21];
        uint8_t nr22 = regs_.data[gb_apu::NR22];
        uint16_t freq = regs_.data[gb_apu::NR23] |
                        ((regs_.data[gb_apu::NR24] & 0x07) << 8);

        ch2_.dac_on = (nr22 & 0xF8) != 0;
        ch2_.enabled = ch2_.dac_on;
        ch2_.duty = (nr21 >> 6) & 0x03;
        ch2_.length_counter = ch2_.length_counter ? ch2_.length_counter : (64 - (nr21 & 0x3F));
        ch2_.length_en = (regs_.data[gb_apu::NR24] & 0x40) != 0;
        ch2_.volume = (nr22 >> 4) & 0x0F;
        ch2_.env_period = nr22 & 0x07;
        ch2_.env_timer = ch2_.env_period;
        ch2_.env_add = (nr22 & 0x08) != 0;
        ch2_.freq_period = (2048 - freq) * 4;
        ch2_.freq_timer = ch2_.freq_period;
    }

    void trigger_ch3() noexcept {
        uint8_t nr30 = regs_.data[gb_apu::NR30];
        uint8_t nr31 = regs_.data[gb_apu::NR31];
        uint8_t nr32 = regs_.data[gb_apu::NR32];
        uint16_t freq = regs_.data[gb_apu::NR33] |
                        ((regs_.data[gb_apu::NR34] & 0x07) << 8);

        ch3_.dac_on = (nr30 & 0x80) != 0;
        ch3_.enabled = ch3_.dac_on;
        ch3_.length_counter = ch3_.length_counter ? ch3_.length_counter : (256 - nr31);
        ch3_.length_en = (regs_.data[gb_apu::NR34] & 0x40) != 0;
        ch3_.output_level = (nr32 >> 5) & 0x03;
        ch3_.freq_period = (2048 - freq) * 2;
        ch3_.freq_timer = ch3_.freq_period;
        ch3_.sample_pos = 0;
    }

    void trigger_ch4() noexcept {
        uint8_t nr41 = regs_.data[gb_apu::NR41];
        uint8_t nr42 = regs_.data[gb_apu::NR42];
        uint8_t nr43 = regs_.data[gb_apu::NR43];

        ch4_.dac_on = (nr42 & 0xF8) != 0;
        ch4_.enabled = ch4_.dac_on;
        ch4_.length_counter = ch4_.length_counter ? ch4_.length_counter : (64 - (nr41 & 0x3F));
        ch4_.length_en = (regs_.data[gb_apu::NR44] & 0x40) != 0;
        ch4_.volume = (nr42 >> 4) & 0x0F;
        ch4_.env_period = nr42 & 0x07;
        ch4_.env_timer = ch4_.env_period;
        ch4_.env_add = (nr42 & 0x08) != 0;
        ch4_.width_mode = (nr43 & 0x08) != 0;
        ch4_.lfsr = 0x7FFF;

        // Noise frequency: divisor_code → base divisor, then shift
        uint8_t divisor_code = nr43 & 0x07;
        uint8_t shift = (nr43 >> 4) & 0x0F;
        static constexpr uint32_t divisors[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };
        ch4_.freq_period = divisors[divisor_code] << shift;
        ch4_.freq_timer = ch4_.freq_period;
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("GB APU — Channel 1 (Square+Sweep)")
            .value("NR10 (Sweep)",  +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR10]; })
            .value("NR12 (Envelope)", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR12]; })
            .flag("Ch1 Active", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->ch1_.enabled ? 1u : 0u; })
            .category("GB APU — Channel 2 (Square)")
            .flag("Ch2 Active", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->ch2_.enabled ? 1u : 0u; })
            .category("GB APU — Channel 3 (Wave)")
            .value("NR30 (Enable)", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR30]; })
            .flag("Ch3 Active", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->ch3_.enabled ? 1u : 0u; })
            .category("GB APU — Channel 4 (Noise)")
            .flag("Ch4 Active", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->ch4_.enabled ? 1u : 0u; })
            .category("GB APU — Master")
            .value("NR50 (Volume)", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR50]; })
            .value("NR52 (Power)",  +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->regs_.data[gb_apu::NR52]; })
            .flag("Power On", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const gb_apu_t*>(c)->power_on_ ? 1u : 0u; });
    }
#endif
};
