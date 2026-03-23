#pragma once

// ============================================================================
// Biquad Audio Filter
// ============================================================================
//
// Generic second-order IIR (biquad) filter for real-time audio processing.
// Supports common filter types: low-pass, high-pass, band-pass, notch,
// peaking EQ, low-shelf, high-shelf.
//
// Usage:
//   BiquadFilter lpf;
//   lpf.set_lowpass(44100.0f, 4000.0f, 0.707f);
//   lpf.process(samples, count);
//
// Thread-safe for single-writer use (one thread calls process()).
// ============================================================================

#include <cmath>
#include <cstdint>

class BiquadFilter {
public:
    enum class Type : uint8_t {
        LowPass,
        HighPass,
        BandPass,
        Notch,
        PeakingEQ,
        LowShelf,
        HighShelf,
    };

    BiquadFilter() = default;

    /// Configure as low-pass filter.
    void set_lowpass(float sample_rate, float cutoff_hz, float q = 0.707f) {
        compute_coefficients(Type::LowPass, sample_rate, cutoff_hz, q, 0.0f);
    }

    /// Configure as high-pass filter.
    void set_highpass(float sample_rate, float cutoff_hz, float q = 0.707f) {
        compute_coefficients(Type::HighPass, sample_rate, cutoff_hz, q, 0.0f);
    }

    /// Configure as band-pass filter.
    void set_bandpass(float sample_rate, float center_hz, float q = 1.0f) {
        compute_coefficients(Type::BandPass, sample_rate, center_hz, q, 0.0f);
    }

    /// Configure as peaking EQ filter.
    void set_peaking(float sample_rate, float center_hz, float q, float gain_db) {
        compute_coefficients(Type::PeakingEQ, sample_rate, center_hz, q, gain_db);
    }

    /// Configure as low-shelf filter.
    void set_lowshelf(float sample_rate, float cutoff_hz, float q, float gain_db) {
        compute_coefficients(Type::LowShelf, sample_rate, cutoff_hz, q, gain_db);
    }

    /// Configure as high-shelf filter.
    void set_highshelf(float sample_rate, float cutoff_hz, float q, float gain_db) {
        compute_coefficients(Type::HighShelf, sample_rate, cutoff_hz, q, gain_db);
    }

    /// Process samples in-place.
    void process(float* samples, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            float x = samples[i];
            float y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
            x2_ = x1_;
            x1_ = x;
            y2_ = y1_;
            y1_ = y;
            samples[i] = y;
        }
    }

    /// Reset filter state (call on discontinuity, e.g. system switch).
    void reset() {
        x1_ = x2_ = y1_ = y2_ = 0.0f;
    }

private:
    // Normalized coefficients (a0 factored out)
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;

    // Filter state
    float x1_ = 0.0f, x2_ = 0.0f;  // input history
    float y1_ = 0.0f, y2_ = 0.0f;  // output history

    void compute_coefficients(Type type, float fs, float f0, float q, float gain_db) {
        constexpr float pi = 3.14159265358979323846f;
        float w0 = 2.0f * pi * f0 / fs;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * q);

        float b0, b1, b2, a0, a1, a2;

        switch (type) {
            case Type::LowPass:
                b1 = 1.0f - cos_w0;
                b0 = b1 * 0.5f;
                b2 = b0;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cos_w0;
                a2 = 1.0f - alpha;
                break;

            case Type::HighPass:
                b1 = -(1.0f + cos_w0);
                b0 = (1.0f + cos_w0) * 0.5f;
                b2 = b0;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cos_w0;
                a2 = 1.0f - alpha;
                break;

            case Type::BandPass:
                b0 = alpha;
                b1 = 0.0f;
                b2 = -alpha;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cos_w0;
                a2 = 1.0f - alpha;
                break;

            case Type::Notch:
                b0 = 1.0f;
                b1 = -2.0f * cos_w0;
                b2 = 1.0f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cos_w0;
                a2 = 1.0f - alpha;
                break;

            case Type::PeakingEQ: {
                float A = std::pow(10.0f, gain_db / 40.0f);
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cos_w0;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cos_w0;
                a2 = 1.0f - alpha / A;
                break;
            }

            case Type::LowShelf: {
                float A = std::pow(10.0f, gain_db / 40.0f);
                float two_sqrt_A_alpha = 2.0f * std::sqrt(A) * alpha;
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + two_sqrt_A_alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - two_sqrt_A_alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + two_sqrt_A_alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0);
                a2 = (A + 1.0f) + (A - 1.0f) * cos_w0 - two_sqrt_A_alpha;
                break;
            }

            case Type::HighShelf: {
                float A = std::pow(10.0f, gain_db / 40.0f);
                float two_sqrt_A_alpha = 2.0f * std::sqrt(A) * alpha;
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + two_sqrt_A_alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - two_sqrt_A_alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + two_sqrt_A_alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0);
                a2 = (A + 1.0f) - (A - 1.0f) * cos_w0 - two_sqrt_A_alpha;
                break;
            }
        }

        // Normalize by a0
        float inv_a0 = 1.0f / a0;
        b0_ = b0 * inv_a0;
        b1_ = b1 * inv_a0;
        b2_ = b2 * inv_a0;
        a1_ = a1 * inv_a0;
        a2_ = a2 * inv_a0;
    }
};

// ============================================================================
// Speaker Simulation Filter Chain
// ============================================================================
//
// Models the frequency response of a small built-in CRT speaker:
//   1. Low-pass at ~5 kHz (small cone can't reproduce highs)
//   2. High-pass at ~150 Hz (no bass extension in a small cabinet)
//   3. Peaking EQ boost at ~1 kHz (cabinet resonance / midrange emphasis)
//
// This produces the characteristic "tinny TV speaker" sound that was the
// primary listening experience for most 8-bit computer users.

class SpeakerSimulation {
public:
    SpeakerSimulation() = default;

    /// Initialize the filter chain for the given sample rate.
    void init(float sample_rate) {
        sample_rate_ = sample_rate;
        lowpass_.set_lowpass(sample_rate, 5000.0f, 0.707f);
        highpass_.set_highpass(sample_rate, 150.0f, 0.707f);
        resonance_.set_peaking(sample_rate, 1000.0f, 1.5f, 4.0f);  // +4 dB at 1 kHz
    }

    /// Process samples in-place through the speaker simulation chain.
    void process(float* samples, uint32_t count) {
        if (sample_rate_ <= 0.0f) return;
        lowpass_.process(samples, count);
        highpass_.process(samples, count);
        resonance_.process(samples, count);
    }

    /// Reset all filter states.
    void reset() {
        lowpass_.reset();
        highpass_.reset();
        resonance_.reset();
    }

    bool is_initialized() const { return sample_rate_ > 0.0f; }

private:
    float sample_rate_ = 0.0f;
    BiquadFilter lowpass_;
    BiquadFilter highpass_;
    BiquadFilter resonance_;
};
