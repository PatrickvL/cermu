#pragma once
/*
 * ay_psg.hpp — AY-3-8910 family Programmable Sound Generator (template)
 *
 * NTTP-parameterized implementation covering the full AY/YM PSG family.
 * Each variant is selected at compile time via AYTraits, enabling
 * zero-overhead feature dispatch with if constexpr.
 *
 * The AY-3-8910 (1978) is a 3-channel square wave + noise + envelope
 * sound generator.  One of the most widely-used sound chips of the 8-bit era.
 *
 * Bus interface:
 *   BDIR + BC1 select the bus operation:
 *     00 = Inactive
 *     01 = Read register
 *     10 = Write register
 *     11 = Latch address
 */

#include "chip/sound/ay_psg/ay_psg_traits.hpp"
#include "chip/sound/sound_chip_base.hpp"
#include "core/signal/audio_port.hpp"
#include "core/system_lines.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// AY-3-8910 REGISTER TABLE — single source of truth
// ============================================================================

// DECL(REG, FLD, CMP) — 16 registers, 18 fields (MIXER/AMP/ENV_SHAPE bits)
#define AY_DECL(REG, FLD, CMP) \
    REG(0x00, TONE_A_FINE,   "Ch A tone period fine")                            \
    REG(0x01, TONE_A_COARSE, "Ch A tone period coarse")                          \
    REG(0x02, TONE_B_FINE,   "Ch B tone period fine")                            \
    REG(0x03, TONE_B_COARSE, "Ch B tone period coarse")                          \
    REG(0x04, TONE_C_FINE,   "Ch C tone period fine")                            \
    REG(0x05, TONE_C_COARSE, "Ch C tone period coarse")                          \
    REG(0x06, NOISE_PERIOD,  "Noise generator period")                           \
    REG(0x07, MIXER,         "Tone/noise mixer control")                         \
      FLD(MIXER, IOB_DIR,   7:7, "I/O port B dir (1=out)",   Flag, 0, 0)        \
      FLD(MIXER, IOA_DIR,   6:6, "I/O port A dir (1=out)",   Flag, 0, 0)        \
      FLD(MIXER, NOISE_C,   5:5, "Noise C disable",          Flag, 0, 0)        \
      FLD(MIXER, NOISE_B,   4:4, "Noise B disable",          Flag, 0, 0)        \
      FLD(MIXER, NOISE_A,   3:3, "Noise A disable",          Flag, 0, 0)        \
      FLD(MIXER, TONE_C,    2:2, "Tone C disable",           Flag, 0, 0)        \
      FLD(MIXER, TONE_B,    1:1, "Tone B disable",           Flag, 0, 0)        \
      FLD(MIXER, TONE_A,    0:0, "Tone A disable",           Flag, 0, 0)        \
    REG(0x08, AMP_A,         "Ch A amplitude")                                   \
      FLD(AMP_A, ENV_A,     4:4, "Envelope mode",            Flag, 0, 0)        \
      FLD(AMP_A, VOL_A,     3:0, "Amplitude",                Value, 0, 0)       \
    REG(0x09, AMP_B,         "Ch B amplitude")                                   \
      FLD(AMP_B, ENV_B,     4:4, "Envelope mode",            Flag, 0, 0)        \
      FLD(AMP_B, VOL_B,     3:0, "Amplitude",                Value, 0, 0)       \
    REG(0x0A, AMP_C,         "Ch C amplitude")                                   \
      FLD(AMP_C, ENV_C,     4:4, "Envelope mode",            Flag, 0, 0)        \
      FLD(AMP_C, VOL_C,     3:0, "Amplitude",                Value, 0, 0)       \
    REG(0x0B, ENV_FINE,      "Envelope period fine")                             \
    REG(0x0C, ENV_COARSE,    "Envelope period coarse")                           \
    REG(0x0D, ENV_SHAPE,     "Envelope shape/cycle")                             \
      FLD(ENV_SHAPE, CONT,  3:3, "Continue",                 Flag, 0, 0)        \
      FLD(ENV_SHAPE, ATT,   2:2, "Attack",                   Flag, 0, 0)        \
      FLD(ENV_SHAPE, ALT,   1:1, "Alternate",                Flag, 0, 0)        \
      FLD(ENV_SHAPE, HOLD,  0:0, "Hold",                     Flag, 0, 0)        \
    REG(0x0E, IO_PORT_A,     "I/O port A data")                                  \
    REG(0x0F, IO_PORT_B,     "I/O port B data")

namespace ay {
namespace reg {
    AY_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr uint8_t REG_COUNT = 16;
}
namespace fld {
#define AY_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
AY_DECL(DECL_REG_NOP, AY_X_FLD_NS_, DECL_CMP_NOP)
#undef AY_X_FLD_NS_
} // namespace fld
} // namespace ay

// Backward compatibility alias
namespace ay_regs = ay::reg;

DECL_EXTRACT(AY, AY_DECL)

// ============================================================================
// AY-3-8914 (Intellivision) register remap table
// ============================================================================
//
// The AY-3-8914 shuffles registers so that the mixer (control) register
// sits at address 0 instead of 7.  The mapping converts external (host-
// facing) addresses to internal (standard AY layout) addresses:
//
//   External 0 → MIXER (internal 7)
//   External 1–6 → TONE_A_FINE … TONE_C_COARSE (internal 0–5)
//   External 7 → NOISE_PERIOD (internal 6)
//   External 8–15 → unchanged (AMP_A … IO_PORT_B)

inline constexpr uint8_t ay_8914_reg_map[16] = {
    0x07,  // ext 0 → MIXER
    0x00,  // ext 1 → TONE_A_FINE
    0x01,  // ext 2 → TONE_A_COARSE
    0x02,  // ext 3 → TONE_B_FINE
    0x03,  // ext 4 → TONE_B_COARSE
    0x04,  // ext 5 → TONE_C_FINE
    0x05,  // ext 6 → TONE_C_COARSE
    0x06,  // ext 7 → NOISE_PERIOD
    0x08,  // ext 8 → AMP_A
    0x09,  // ext 9 → AMP_B
    0x0A,  // ext 10 → AMP_C
    0x0B,  // ext 11 → ENV_FINE
    0x0C,  // ext 12 → ENV_COARSE
    0x0D,  // ext 13 → ENV_SHAPE
    0x0E,  // ext 14 → IO_PORT_A
    0x0F,  // ext 15 → IO_PORT_B
};

// ============================================================================
// ay_psg_t — AY/YM PSG family template
// ============================================================================

template <const AYTraits& Traits>
class ay_psg_t : public SoundChipBase {
public:
    ay_psg_t()
        : SoundChipBase(ChipInfo(Traits.chip_id, Traits.vendor))
        , audio_buffer_(4096)
    {
        init_regs(ay_regs::REG_COUNT);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void init() {
        std::memset(regs_, 0, num_regs_);
        latch_addr_ = 0;
        for (auto& ch : tone_counter_) ch = 0;
        for (auto& ch : tone_output_)  ch = 0;
        noise_counter_ = 0;
        noise_shift_ = 1;  // LFSR seed
        env_counter_ = 0;
        env_step_ = 0;
        env_volume_ = 0;
        env_ascending_ = false;
        env_holding_ = false;
        if constexpr (Traits.has_io_port_a()) io_port_a_ = 0xFF;
        if constexpr (Traits.has_io_port_b()) io_port_b_ = 0xFF;
        audio_buffer_.reset();
        audio_cycle_accum_ = 0.0;
        audio_cycles_per_sample_ = 0.0;
    }

    void reset() { init(); }

    // === Register address mapping ===

    /// Map an external register address to the internal (standard) layout.
    /// Identity for all variants except AY-3-8914 (Intellivision remap).
    static constexpr uint8_t map_reg(uint8_t addr) {
        if constexpr (Traits.has_register_remap()) {
            return ay_8914_reg_map[addr & 0x0F];
        } else {
            return addr & 0x0F;
        }
    }

    // === Register access (address latch + read/write) ===

    void latch_address(uint8_t addr) {
        latch_addr_ = map_reg(addr);
    }

    void write_register(uint8_t data) {
        regs_[latch_addr_] = data;
        on_register_write(latch_addr_, data);
    }

    /// Direct addressed write (for AudioThread adapter).
    /// Does NOT touch latch_addr_ — safe for audio-thread use while the
    /// emu thread owns latch_addr_ for read_register() / latch_address().
    void write_register(uint8_t reg, uint8_t data) {
        uint8_t r = map_reg(reg);
        regs_[r] = data;
        on_register_write(r, data);
    }

    /// Shadow write — updates regs_[] only (no side effects).
    /// Call from the emu thread so read_register() returns current data,
    /// while the full write (with envelope reset etc.) is enqueued for
    /// the audio thread.
    void write_register_shadow(uint8_t data) {
        regs_[latch_addr_] = data;
    }

    uint8_t read_register() const {
        if constexpr (Traits.has_io_port_a()) {
            if (latch_addr_ == ay_regs::IO_PORT_A) return io_port_a_;
        }
        if constexpr (Traits.has_io_port_b()) {
            if (latch_addr_ == ay_regs::IO_PORT_B) return io_port_b_;
        }
        // Variants without the requested I/O port return the register
        // contents (which default to 0x00 after reset).
        return regs_[latch_addr_];
    }

    // === I/O port access (directly from system, not through register bus) ===

    void set_io_port_a(uint8_t data) {
        if constexpr (Traits.has_io_port_a()) { io_port_a_ = data; }
    }
    void set_io_port_b(uint8_t data) {
        if constexpr (Traits.has_io_port_b()) { io_port_b_ = data; }
    }
    uint8_t get_io_port_a() const {
        if constexpr (Traits.has_io_port_a()) return io_port_a_;
        else return 0xFF;
    }
    uint8_t get_io_port_b() const {
        if constexpr (Traits.has_io_port_b()) return io_port_b_;
        else return 0xFF;
    }

    // === Audio generation ===

    /// Tick at the AY clock rate (typically 1.7734 MHz or CPU_CLK/2).
    /// Advances tone, noise, and envelope generators by one step.
    void tick() {
        // --- Tone generators (3 channels) ---
        for (int ch = 0; ch < 3; ch++) {
            uint16_t period = ((regs_[ch * 2 + 1] & 0x0F) << 8) | regs_[ch * 2];
            if (period == 0) period = 1;
            if (++tone_counter_[ch] >= period) {
                tone_counter_[ch] = 0;
                tone_output_[ch] ^= 1;
            }
        }

        // --- Noise generator (17-bit LFSR) ---
        {
            uint8_t np = regs_[ay_regs::NOISE_PERIOD] & 0x1F;
            if (np == 0) np = 1;
            if (++noise_counter_ >= np) {
                noise_counter_ = 0;
                // 17-bit LFSR: feedback = bit0 XOR bit3
                uint32_t fb = ((noise_shift_ ^ (noise_shift_ >> 3)) & 1);
                noise_shift_ = (noise_shift_ >> 1) | (fb << 16);
            }
        }

        // --- Envelope generator ---
        if (!env_holding_) {
            uint16_t ep = (regs_[ay_regs::ENV_COARSE] << 8) | regs_[ay_regs::ENV_FINE];
            if (ep == 0) ep = 1;
            if (++env_counter_ >= ep) {
                env_counter_ = 0;
                advance_envelope();
            }
        }

        // Emit decimated sample into ring buffer (if sample rate configured)
        buffer_sample();
    }

    /// Get mixed mono sample (float, -1.0 to +1.0).
    /// Used for direct polling (e.g. Spectrum beeper+AY mix).
    float get_sample() const {
        uint8_t mixer = regs_[ay_regs::MIXER];
        float mix = 0.0f;

        for (int ch = 0; ch < 3; ch++) {
            // Mixer bits are active-low: 0 = enabled
            // When disabled, the signal is forced HIGH (always "on")
            bool tone_out  = (mixer & (1 << ch))       ? true : (bool)tone_output_[ch];
            bool noise_out = (mixer & (1 << (ch + 3))) ? true : (bool)(noise_shift_ & 1);
            // Channel contributes volume when BOTH gates are high
            if (tone_out && noise_out) {
                uint8_t amp_reg = regs_[ay_regs::AMP_A + ch];
                bool env_mode = amp_reg & 0x10;
                uint8_t level = env_mode ? env_volume_ : (amp_reg & 0x0F);
                mix += dac_table_[level & 0x0F];
            }
        }

        // 3 channels each max 1.0 → normalize to [-1.0, +1.0]
        return (mix / 3.0f) * 2.0f - 1.0f;
    }

    // === Audio output (ring buffer for systems using chip-level drain) ===

    void set_clock_frequency(uint32_t internal_hz) {
        internal_clock_hz_ = internal_hz;
        update_cycles_per_sample();
    }

    void set_audio_sample_rate(int sample_rate_hz) {
        audio_sample_rate_ = sample_rate_hz;
        update_cycles_per_sample();
    }

    void set_audio_port(AudioPort* port) { audio_port_ = port; }

    uint32_t audio_read(float* buffer, uint32_t max_samples) {
        return audio_buffer_.read(buffer, max_samples);
    }

    uint32_t audio_available() const {
        return audio_buffer_.available();
    }

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    uint8_t   latch_addr_ = 0;

    // Tone generators (3 channels)
    uint16_t  tone_counter_[3]{};
    uint8_t   tone_output_[3]{};

    // Noise generator (17-bit LFSR)
    uint16_t  noise_counter_ = 0;
    uint32_t  noise_shift_ = 1;

    // Envelope generator
    uint16_t  env_counter_ = 0;
    uint8_t   env_step_ = 0;
    uint8_t   env_volume_ = 0;
    bool      env_ascending_ = false;
    bool      env_holding_ = false;

    // I/O ports (always present in memory; gated by if constexpr in public API)
    uint8_t   io_port_a_ = 0xFF;
    uint8_t   io_port_b_ = 0xFF;

    // Audio output — decimated from AY clock to audio sample rate
    AudioRingBuffer audio_buffer_;
    AudioPort* audio_port_ = nullptr;  // Optional analog signal output
    uint32_t internal_clock_hz_ = 0;
    int      audio_sample_rate_ = 0;
    double   audio_cycles_per_sample_ = 0.0;
    double   audio_cycle_accum_ = 0.0;

    // AY-3-8910 DAC table — logarithmic 16-level amplitude
    // Values measured from real hardware (Matthew Westcott)
    static constexpr float dac_table_[16] = {
        0.0000f, 0.0137f, 0.0205f, 0.0291f, 0.0423f, 0.0618f, 0.0847f, 0.1369f,
        0.1691f, 0.2647f, 0.3527f, 0.4499f, 0.5704f, 0.6873f, 0.8482f, 1.0000f
    };

    void update_cycles_per_sample() {
        if (audio_sample_rate_ > 0 && internal_clock_hz_ > 0) {
            audio_cycles_per_sample_ =
                static_cast<double>(internal_clock_hz_) / audio_sample_rate_;
        }
    }

    /// Emit a decimated sample into the ring buffer if sample rate is configured.
    void buffer_sample() {
        if (audio_cycles_per_sample_ <= 0.0) return;  // No sample rate → skip
        audio_cycle_accum_ += 1.0;
        if (audio_cycle_accum_ < audio_cycles_per_sample_) return;
        audio_cycle_accum_ -= audio_cycles_per_sample_;
        float sample = get_sample();
        if (audio_port_) {
            audio_port_->drive_sample(sample);
        } else {
            audio_buffer_.write(&sample, 1);
        }
    }

    /// Side effects triggered by writing to a specific internal register.
    void on_register_write(uint8_t internal_reg, uint8_t data) {
        if (internal_reg == ay_regs::ENV_SHAPE) {
            // Writing envelope shape resets the envelope generator
            env_step_ = 0;
            env_counter_ = 0;
            env_holding_ = false;
            env_ascending_ = (data & 0x04) != 0;  // ATT bit
            env_volume_ = env_ascending_ ? 0 : Traits.envelope_max();
            // Clamp to DAC range for half-step variants
            if constexpr (Traits.has_half_step_envelope()) {
                env_volume_ >>= 1;
            }
        }
    }

    /// Advance the envelope generator one step.
    void advance_envelope() {
        env_step_++;
        constexpr uint8_t steps = Traits.envelope_steps;

        if (env_step_ < steps) {
            if constexpr (Traits.has_half_step_envelope()) {
                // YM2149/YM3439: 32 steps, output mapped to 0-15 for DAC lookup
                uint8_t raw = env_ascending_ ? env_step_ : (uint8_t(steps - 1) - env_step_);
                env_volume_ = raw >> 1;
            } else {
                // AY: 16 steps, direct 0-15 mapping
                env_volume_ = env_ascending_ ? env_step_ : (15 - env_step_);
            }
            return;
        }

        // End of a full cycle — handle shape
        uint8_t shape = regs_[ay_regs::ENV_SHAPE] & 0x0F;
        bool cont = shape & 0x08;
        bool alt  = shape & 0x02;
        bool hold = shape & 0x01;

        if (!cont) {
            // Non-continue: volume drops to 0
            env_volume_ = 0;
            env_holding_ = true;
        } else if (hold) {
            // Continue + hold: stay at final level
            env_volume_ = (env_ascending_ != (bool)alt) ? 15 : 0;
            env_holding_ = true;
        } else if (alt) {
            // Continue + alternate: reverse direction, restart
            env_ascending_ = !env_ascending_;
            env_step_ = 0;
            if constexpr (Traits.has_half_step_envelope()) {
                env_volume_ = env_ascending_ ? 0 : 15;
            } else {
                env_volume_ = env_ascending_ ? 0 : 15;
            }
        } else {
            // Continue + restart same direction (sawtooth)
            env_step_ = 0;
            env_volume_ = env_ascending_ ? 0 : 15;
        }
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};
