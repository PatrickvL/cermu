#pragma once
/*
 * sn76489.h — Texas Instruments SN76489 Digital Complex Sound Generator
 *
 * The SN76489 (1980) is a 4-channel sound chip: 3 square-wave tone
 * generators and 1 noise generator, each with independent 4-bit
 * attenuation (volume).
 *
 * Variants:
 *   SN76489  — Original TI (16-pin DIP), used in BBC Micro, TI-99/4A
 *   SN76489A — Revised (different noise taps: 0x0009 instead of 0x0003)
 *   SN76496  — Pin-compatible variant used in Sega arcade boards
 *   Sega PSG — Integrated into Sega Master System / Game Gear VDP
 *              (uses SN76489A noise taps: 0x0009)
 *
 * Used in: BBC Micro, Sega Master System, Sega Game Gear, Sega Genesis
 *          (secondary PSG), ColecoVision, TI-99/4A, IBM PCjr, Tandy 1000,
 *          many Sega/Konami arcade boards.
 *
 * Register interface:
 *   Write-only via a single data bus latch.  Bits [7:4] select the
 *   register+channel; bit 7 = 1 means "latch" (new channel/type), bit 7 = 0
 *   means "data" (continues writing to previously latched register).
 *
 *   Latch byte format: 1 cc t dddd
 *     cc  = channel (0-3; 3 = noise)
 *     t   = 0: tone/noise register, 1: volume/attenuation register
 *     dddd = low 4 bits of data
 *
 *   Data byte format: 0 x dddddd
 *     dddddd = high 6 bits of tone period (concatenated with latched low 4)
 *
 *   Noise register ($E0): bits [2:0]
 *     bit 2:   feedback type (0 = periodic, 1 = white noise)
 *     bits 1-0: shift rate (0-2 = fixed dividers, 3 = Channel 2 tone)
 *
 * Clock: typically the master clock / 16 internally (3.579545 MHz master
 *        on NTSC Sega, 4 MHz on BBC Micro).  The internal divider halves
 *        this further for the 10-bit tone counters.
 *
 * Output: 4-bit attenuation per channel → DAC → single mono output.
 *   Attenuation: 0 = full volume, 15 = silence.  Each step ≈ 2 dB.
 */

#include "../../../core/chip.h"
#include "../../../utils/ring_buffer.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// SN76489 Variant Configuration
// ============================================================================

enum class SN76489Variant : uint8_t {
    SN76489,      // Original TI — noise taps: bits 0,1 (0x0003)
    SN76489A,     // Revised TI  — noise taps: bits 0,3 (0x0009)
    SN76496,      // TI variant for Sega arcade
    SEGA_PSG,     // Sega VDP-integrated (SN76489A taps)
};

struct SN76489VariantTraits {
    const char* part_number;
    const char* manufacturer;
    uint16_t    noise_taps;     // LFSR feedback tap mask (XOR bits)
    uint16_t    noise_bit;      // Width of LFSR (bit position of MSB)
};

inline constexpr SN76489VariantTraits sn76489_variant_traits[] = {
    { "SN76489",  "Texas Instruments", 0x0003, 15 },  // 16-bit LFSR, taps 0+1
    { "SN76489A", "Texas Instruments", 0x0009, 15 },  // 16-bit LFSR, taps 0+3
    { "SN76496",  "Texas Instruments", 0x0009, 15 },  //   (same as A)
    { "Sega PSG", "Sega",              0x0009, 15 },  // Integrated in VDP
};

// ============================================================================
// SN76489 Sound Chip
// ============================================================================

class sn76489_t : public ChipBase {
public:
    explicit sn76489_t(SN76489Variant variant = SN76489Variant::SN76489)
        : ChipBase(ChipInfo(
              sn76489_variant_traits[static_cast<int>(variant)].part_number,
              sn76489_variant_traits[static_cast<int>(variant)].manufacturer))
        , variant_(variant)
        , audio_buffer_(4096)
    {
        category_ = "Sound";
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    void init() {
        for (int i = 0; i < 3; i++) {
            tone_reg_[i] = 0;
            tone_counter_[i] = 0;
            tone_output_[i] = 1;
            volume_[i] = 0x0F;  // Silence
        }
        noise_reg_ = 0;
        noise_counter_ = 0;
        noise_shift_ = 0x8000;  // LFSR initial state
        volume_[3] = 0x0F;      // Noise channel silent
        latched_channel_ = 0;
        latched_is_volume_ = false;

        audio_buffer_.reset();
        audio_cycle_accum_ = 0.0;
        audio_cycles_per_sample_ = 0.0;
    }

    void reset() { init(); }

    // ========================================================================
    // REGISTER WRITE (active-low active _WE pin)
    // ========================================================================

    /// Write a byte to the SN76489 data bus.
    /// Called when /WE goes low (active-low write enable).
    void write(uint8_t data) {
        if (data & 0x80) {
            // Latch byte: 1 cc t dddd
            latched_channel_   = (data >> 5) & 0x03;
            latched_is_volume_ = (data >> 4) & 0x01;

            if (latched_is_volume_) {
                volume_[latched_channel_] = data & 0x0F;
            } else {
                if (latched_channel_ < 3) {
                    // Tone: set low 4 bits, preserve high 6
                    tone_reg_[latched_channel_] =
                        (tone_reg_[latched_channel_] & 0x3F0) | (data & 0x0F);
                } else {
                    // Noise register
                    noise_reg_ = data & 0x07;
                    noise_shift_ = 0x8000;  // Reset LFSR on noise reg write
                }
            }
        } else {
            // Data byte: 0 x dddddd — continues last latched register
            if (latched_is_volume_) {
                volume_[latched_channel_] = data & 0x0F;
            } else {
                if (latched_channel_ < 3) {
                    // Tone: set high 6 bits, preserve low 4
                    tone_reg_[latched_channel_] =
                        (tone_reg_[latched_channel_] & 0x00F) | ((data & 0x3F) << 4);
                } else {
                    noise_reg_ = data & 0x07;
                    noise_shift_ = 0x8000;
                }
            }
        }
    }

    // ========================================================================
    // AUDIO GENERATION
    // ========================================================================

    /// Tick at the chip's internal clock rate (master_clock / 16).
    /// Call this once per internal clock cycle.
    void tick() {
        // --- Tone channels 0-2 ---
        for (int ch = 0; ch < 3; ch++) {
            if (tone_counter_[ch] > 0) {
                tone_counter_[ch]--;
            } else {
                // Reload and flip output
                tone_counter_[ch] = tone_reg_[ch];
                tone_output_[ch] ^= 1;
            }
        }

        // --- Noise channel ---
        if (noise_counter_ > 0) {
            noise_counter_--;
        } else {
            // Reload rate
            switch (noise_reg_ & 0x03) {
                case 0: noise_counter_ = 0x10;  break;  // N/512
                case 1: noise_counter_ = 0x20;  break;  // N/1024
                case 2: noise_counter_ = 0x40;  break;  // N/2048
                case 3: noise_counter_ = tone_reg_[2]; break; // Channel 2 period
            }

            // Shift LFSR
            const auto& traits = sn76489_variant_traits[static_cast<int>(variant_)];
            bool feedback;
            if (noise_reg_ & 0x04) {
                // White noise: XOR of tapped bits
                uint16_t tapped = noise_shift_ & traits.noise_taps;
                feedback = __builtin_parity(tapped);
            } else {
                // Periodic noise: bit 0 only
                feedback = noise_shift_ & 1;
            }
            noise_shift_ = (noise_shift_ >> 1) | (feedback ? (1 << traits.noise_bit) : 0);
        }

        // --- Mix and produce audio sample ---
        generate_sample();
    }

    // ========================================================================
    // AUDIO OUTPUT
    // ========================================================================

    /// Set the target audio sample rate (call when SDL audio opens).
    void set_audio_sample_rate(int sample_rate_hz) {
        if (sample_rate_hz > 0 && internal_clock_hz_ > 0) {
            audio_cycles_per_sample_ =
                static_cast<double>(internal_clock_hz_) / sample_rate_hz;
        }
    }

    /// Set the internal clock frequency (master_clock / 16).
    void set_clock_frequency(uint32_t internal_hz) {
        internal_clock_hz_ = internal_hz;
    }

    /// Read audio samples into buffer. Returns number of samples written.
    uint32_t audio_read(float* buffer, uint32_t max_samples) {
        return audio_buffer_.read(buffer, max_samples);
    }

    /// Number of audio samples available.
    uint32_t audio_available() const {
        return audio_buffer_.available();
    }

    // ========================================================================
    // STATE ACCESSORS (for debug display)
    // ========================================================================

    uint16_t tone_register(int ch) const { return (ch < 3) ? tone_reg_[ch] : 0; }
    uint8_t  volume(int ch) const { return (ch < 4) ? volume_[ch] : 0; }
    uint8_t  noise_register() const { return noise_reg_; }
    uint16_t noise_shift() const { return noise_shift_; }

private:
    SN76489Variant variant_;

    // Tone generators (3 channels)
    uint16_t tone_reg_[3]{};        // 10-bit tone period registers
    uint16_t tone_counter_[3]{};    // Current countdown value
    uint8_t  tone_output_[3]{};     // Current output state (0 or 1)

    // Noise generator
    uint8_t  noise_reg_ = 0;       // 3-bit noise control (FB | Rate1 | Rate0)
    uint16_t noise_counter_ = 0;   // Current countdown
    uint16_t noise_shift_ = 0x8000; // 16-bit LFSR state

    // Volume (attenuation) — 4 channels (tone0, tone1, tone2, noise)
    uint8_t  volume_[4]{};          // 0 = max volume, 15 = silence

    // Latch state
    uint8_t  latched_channel_ = 0;
    bool     latched_is_volume_ = false;

    // Audio output
    AudioRingBuffer audio_buffer_;
    uint32_t internal_clock_hz_ = 0;
    double   audio_cycles_per_sample_ = 0.0;
    double   audio_cycle_accum_ = 0.0;

    // Attenuation table: 2 dB per step, linear voltage
    // volume_table[0] = max, volume_table[15] = 0 (silence)
    static constexpr float volume_table_[16] = {
        1.0f, 0.7943f, 0.6310f, 0.5012f, 0.3981f, 0.3162f, 0.2512f, 0.1995f,
        0.1585f, 0.1259f, 0.1000f, 0.0794f, 0.0631f, 0.0501f, 0.0398f, 0.0f
    };

    void generate_sample() {
        audio_cycle_accum_ += 1.0;
        if (audio_cycles_per_sample_ <= 0.0) return;
        if (audio_cycle_accum_ < audio_cycles_per_sample_) return;
        audio_cycle_accum_ -= audio_cycles_per_sample_;

        // Mix all 4 channels
        float sample = 0.0f;
        for (int ch = 0; ch < 3; ch++) {
            sample += tone_output_[ch] ? volume_table_[volume_[ch]] : 0.0f;
        }
        // Noise channel
        sample += (noise_shift_ & 1) ? volume_table_[volume_[3]] : 0.0f;

        // Scale: 4 channels each contributing 0..1.0 → normalize to -1..+1
        sample = (sample / 4.0f) * 2.0f - 1.0f;

        audio_buffer_.write(&sample, 1);
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        auto& dbg = debug_registry_;

        dbg.category("Tone Channels")
            .value("Ch0 Period", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->tone_reg_[0]; })
            .value("Ch1 Period", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->tone_reg_[1]; })
            .value("Ch2 Period", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->tone_reg_[2]; });

        dbg.category("Attenuation")
            .value("Ch0 Vol", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->volume_[0]; })
            .value("Ch1 Vol", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->volume_[1]; })
            .value("Ch2 Vol", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->volume_[2]; })
            .value("Noise Vol", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->volume_[3]; });

        dbg.category("Noise")
            .value("Noise Reg", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->noise_reg_; })
            .flag("White Noise", [](const ChipBase* c) -> uint32_t {
                return (static_cast<const sn76489_t*>(c)->noise_reg_ & 0x04) != 0; })
            .value("LFSR", [](const ChipBase* c) -> uint32_t {
                return static_cast<const sn76489_t*>(c)->noise_shift_; });
    }
#endif
};
