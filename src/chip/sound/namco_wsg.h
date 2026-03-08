#pragma once
/*
 * namco_wsg.h — Namco WSG (Waveform Sound Generator)
 *
 * The Namco WSG3 is a custom 3-channel wavetable sound generator used in
 * Pac-Man (1980), and the 8-channel WSG8 variant is used in later games
 * such as Galaga, Dig Dug and Pole Position (1982-83).
 *
 * Pengo (1982) uses the same 3-channel WSG as Pac-Man.
 *
 * Each channel generates a sound by stepping through a 32-sample × 4-bit
 * waveform at a programmable frequency.  Amplitude is 4-bit per channel.
 *
 * Memory-mapped register interface:
 *   WSG3 (Pac-Man/Pengo):
 *     $00-$04:  Voice 1 frequency (5 bytes, 20-bit accumulator)
 *     $05-$09:  Voice 2 frequency
 *     $0A-$0E:  Voice 3 frequency
 *     $0F:      Voice 1 waveform select + volume
 *     $10:      Voice 2 waveform select + volume
 *     $14:      Voice 3 waveform select + volume
 *     Waveform ROM: 8 waveforms × 32 samples × 4-bit
 *
 * Used in: Pac-Man, Ms. Pac-Man, Pengo, Mappy (WSG3)
 *          Galaga, Dig Dug, Pole Position, Xevious (WSG8 variant)
 */

#include "../../core/chip.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// ============================================================================
// Namco WSG Variant
// ============================================================================

enum class WSGVariant : uint8_t {
    WSG3,   // 3-channel (Pac-Man, Pengo)
    WSG8,   // 8-channel (Galaga, etc.)
};

// ============================================================================
// Namco WSG Register Addresses (memory-mapped)
// ============================================================================

namespace wsg_regs {
    // Voice 1 frequency (5 × 4-bit nibbles)
    constexpr uint8_t V1_FREQ0    = 0x00;
    constexpr uint8_t V1_FREQ4    = 0x04;
    // Voice 2 frequency
    constexpr uint8_t V2_FREQ0    = 0x05;
    constexpr uint8_t V2_FREQ4    = 0x09;
    // Voice 3 frequency
    constexpr uint8_t V3_FREQ0    = 0x0A;
    constexpr uint8_t V3_FREQ4    = 0x0E;
    // Waveform + volume per voice
    constexpr uint8_t V1_WAVEVOL  = 0x0F;
    constexpr uint8_t V2_WAVEVOL  = 0x10;
    constexpr uint8_t V3_WAVEVOL  = 0x14;
    constexpr uint8_t REG_COUNT   = 0x15;
} // namespace wsg_regs

// ============================================================================
// Namco WSG Sound Generator
// ============================================================================

class namco_wsg_t : public ChipBase {
public:
    explicit namco_wsg_t(WSGVariant variant = WSGVariant::WSG3)
        : ChipBase(ChipInfo(
              variant == WSGVariant::WSG3 ? "WSG3" : "WSG8",
              "Namco"))
        , variant_(variant)
        , num_channels_(variant == WSGVariant::WSG3 ? 3 : 8)
    {
        category_ = "Sound";
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void init() {
        for (auto& ch : channels_) {
            ch = {};
        }
        std::memset(regs_, 0, sizeof(regs_));
        std::memset(waveform_rom_, 0, sizeof(waveform_rom_));
    }

    void reset() { init(); }

    // === Waveform ROM loading ===

    /// Load waveform ROM data (256 bytes: 8 waveforms × 32 samples × 4-bit packed)
    void load_waveform_rom(const uint8_t* data, size_t size) {
        size_t copy = (size < sizeof(waveform_rom_)) ? size : sizeof(waveform_rom_);
        std::memcpy(waveform_rom_, data, copy);
    }

    // === Register writes (memory-mapped from main bus) ===

    /// Write a frequency register byte.
    void write_freq(int channel, int byte_idx, uint8_t data) {
        if (channel >= num_channels_ || byte_idx >= 5) return;
        channels_[channel].freq[byte_idx] = data & 0x0F;
        // Mirror to register array
        uint8_t reg_idx = static_cast<uint8_t>(channel * 5 + byte_idx);
        if (reg_idx < wsg_regs::REG_COUNT) regs_[reg_idx] = data & 0x0F;
    }

    /// Set waveform select and volume for a channel.
    void write_wave_vol(int channel, uint8_t waveform, uint8_t volume) {
        if (channel >= num_channels_) return;
        channels_[channel].waveform = waveform & 0x07;
        channels_[channel].volume   = volume & 0x0F;
        // Mirror to register array
        static constexpr uint8_t wavevol_regs[] = {
            wsg_regs::V1_WAVEVOL, wsg_regs::V2_WAVEVOL, wsg_regs::V3_WAVEVOL
        };
        if (channel < 3) regs_[wavevol_regs[channel]] = ((waveform & 0x07) << 4) | (volume & 0x0F);
    }

    // === Audio tick ===

    /// Tick at the WSG clock rate (~96 kHz for Pac-Man: 18.432 MHz / 192).
    void tick() {
        for (int i = 0; i < num_channels_; ++i) {
            auto& ch = channels_[i];
            // Build 20-bit frequency from 5 nibbles
            uint32_t freq = 0;
            for (int b = 4; b >= 0; --b) {
                freq = (freq << 4) | ch.freq[b];
            }
            ch.accumulator = (ch.accumulator + freq) & 0xFFFFF;
        }
    }

    /// Get mixed mono sample (float, -1.0 to +1.0).
    float get_sample() const {
        int32_t mix = 0;
        for (int i = 0; i < num_channels_; ++i) {
            const auto& ch = channels_[i];
            if (ch.volume == 0) continue;
            // 5-bit sample index from accumulator bits 15-19
            uint8_t sample_idx = (ch.accumulator >> 15) & 0x1F;
            uint8_t wave_offset = ch.waveform * 16 + (sample_idx >> 1);
            uint8_t packed = waveform_rom_[wave_offset];
            uint8_t sample_4bit = (sample_idx & 1) ? (packed >> 4) : (packed & 0x0F);
            // Signed: 4-bit sample centered at 8
            int8_t signed_sample = static_cast<int8_t>(sample_4bit) - 8;
            mix += signed_sample * ch.volume;
        }
        // Normalize: max possible = 8 × 7 × 15 = 840
        return static_cast<float>(mix) / 840.0f;
    }

    WSGVariant variant()     const { return variant_; }
    int        num_channels() const { return num_channels_; }

private:
    struct Channel {
        uint8_t  freq[5]{};        // 5 × 4-bit frequency nibbles
        uint32_t accumulator = 0;  // 20-bit phase accumulator
        uint8_t  waveform = 0;     // 3-bit waveform select (0-7)
        uint8_t  volume = 0;       // 4-bit volume
    };

    WSGVariant variant_;
    int        num_channels_;
    Channel    channels_[8]{};        // Max 8 channels (WSG8)
    uint8_t    regs_[wsg_regs::REG_COUNT]{};  // Register mirror
    uint8_t    waveform_rom_[256]{};  // 8 waveforms × 32 nibble-samples (packed)

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using S = const namco_wsg_t;
        auto& r = debug_registry_;
        r.set_registers(regs_, wsg_regs::REG_COUNT);

        r.category("Voice 1");
        r.value("Frequency", +[](const ChipBase* c) -> uint32_t {
            auto* s = static_cast<S*>(c);
            uint32_t f = 0;
            for (int b = 4; b >= 0; --b) f = (f << 4) | s->channels_[0].freq[b];
            return f;
        }, 20);
        r.value("Waveform", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channels_[0].waveform;
        }, 3);
        r.value("Volume", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channels_[0].volume;
        }, 4);

        r.category("Voice 2");
        r.value("Frequency", +[](const ChipBase* c) -> uint32_t {
            auto* s = static_cast<S*>(c);
            uint32_t f = 0;
            for (int b = 4; b >= 0; --b) f = (f << 4) | s->channels_[1].freq[b];
            return f;
        }, 20);
        r.value("Waveform", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channels_[1].waveform;
        }, 3);
        r.value("Volume", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channels_[1].volume;
        }, 4);

        r.category("Voice 3");
        r.value("Frequency", +[](const ChipBase* c) -> uint32_t {
            auto* s = static_cast<S*>(c);
            uint32_t f = 0;
            for (int b = 4; b >= 0; --b) f = (f << 4) | s->channels_[2].freq[b];
            return f;
        }, 20);
        r.value("Waveform", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channels_[2].waveform;
        }, 3);
        r.value("Volume", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channels_[2].volume;
        }, 4);
    }
#endif
};
