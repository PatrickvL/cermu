#pragma once

// ============================================================================
// AudioStream — chip-facing write interface for audio signal output
// ============================================================================
//
// The chip calls drive(value) once per chip clock.  The stream accumulates
// samples and decimates to the host sample rate using simple box-car
// downsampling with optional BLEP correction for band-limited transitions.
//
// Value range: chip-specific float amplitude (e.g. 0.0 - 1.0, or DAC units).
// The stream normalizes to host [-1.0, +1.0] range on output.
// ============================================================================

#include "core/cermu.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>

// ============================================================================
// Lock-free ring buffer for audio thread decoupling
// ============================================================================

struct alignas(64) AudioRing {
    static constexpr int CAP = 4096;    // power of 2 — enables & mask
    float    buf_[CAP] = {};
    alignas(64) std::atomic<uint32_t> write_{0};
    alignas(64) std::atomic<uint32_t> read_{0};

    // Producer (emulator thread) — never blocks
    void push_unchecked(float s) noexcept {
        uint32_t w = write_.load(std::memory_order_relaxed);
        buf_[w & (CAP - 1)] = s;
        write_.store(w + 1, std::memory_order_release);
    }

    // Consumer (audio callback)
    int pop(float* dst, int n) noexcept {
        uint32_t r = read_.load(std::memory_order_relaxed);
        uint32_t w = write_.load(std::memory_order_acquire);
        int avail = static_cast<int>(w - r);
        if (avail < 0) avail = 0;
        if (avail > n) avail = n;
        for (int i = 0; i < avail; i++)
            dst[i] = buf_[(r + i) & (CAP - 1)];
        read_.store(r + avail, std::memory_order_release);
        return avail;
    }

    uint32_t available() const noexcept {
        uint32_t r = read_.load(std::memory_order_relaxed);
        uint32_t w = write_.load(std::memory_order_acquire);
        return w - r;
    }
};

// ============================================================================
// AudioPort — accumulates chip-rate samples, decimates to host rate
// ============================================================================
//
// Drive model: the chip calls drive(value) once per chip clock.
// When enough chip clocks have accumulated to fill one host sample,
// the averaged value is emitted to the ring buffer.
//
// Future: BLEP correction for band-limited step transitions.
// ============================================================================

struct AudioPort {
    float     accumulator_  = 0.f;
    uint32_t  acc_count_    = 0;
    uint32_t  period_       = 1;        // chip clocks per host sample
    float     prev_         = 0.f;
    float     scale_        = 1.f;      // output scaling factor
    AudioRing ring_;

    /// Configure the decimation ratio.
    /// chip_clock_hz:  chip oscillator frequency (e.g. 1108405 for VIC-20 PAL)
    /// host_sample_hz: host audio output rate (e.g. 44100)
    void configure(uint32_t chip_clock_hz, uint32_t host_sample_hz) noexcept {
        if (host_sample_hz == 0) host_sample_hz = 44100;
        period_ = chip_clock_hz / host_sample_hz;
        if (period_ == 0) period_ = 1;
    }

    /// Set output scaling factor (maps chip DAC range to [-1,+1])
    void set_scale(float s) noexcept { scale_ = s; }

    /// Called once per chip clock with the current output amplitude.
    FORCE_INLINE
    void drive(float value) noexcept {
        accumulator_ += value;

        if (++acc_count_ >= period_) {
            float s = (accumulator_ / static_cast<float>(acc_count_)) * scale_;
            ring_.push_unchecked(s);
            acc_count_   = 0;
            accumulator_ = 0.f;
        }

        prev_ = value;
    }

    /// Push a pre-decimated sample directly to the ring buffer.
    /// For chips with sophisticated internal downsampling (e.g. SID CIC-3)
    /// that already produce samples at the host sample rate.
    FORCE_INLINE
    void drive_sample(float value) noexcept {
        ring_.push_unchecked(value * scale_);
    }

    /// Read samples into host buffer.  Returns number of samples written.
    int read_samples(float* dst, int max_samples) noexcept {
        return ring_.pop(dst, max_samples);
    }

    /// Number of samples available for reading.
    uint32_t available() const noexcept {
        return ring_.available();
    }
};
