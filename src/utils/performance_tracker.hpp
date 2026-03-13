#pragma once
/**
 * performance_tracker.hpp — Generic fixed-size time-series performance tracker
 *
 * Records timestamped samples in a zero-allocation circular buffer and
 * maintains running statistics (SMA, EMA, stddev, min, max) over a
 * configurable sliding time window.
 *
 * Designed for per-frame metrics (frame time, frame interval, audio
 * buffer fill %) but usable for any scalar time-series.
 *
 * Thread safety: NOT thread-safe.  All calls must be serialised
 * externally (e.g. under emu_mutex_ or from a single thread).
 *
 * Persistence: samples can be streamed out to CSV or binary format.
 * Logging is opt-in and off by default.
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>

template <size_t MaxSamples = 4096>
class PerformanceTracker {
public:
    struct Sample {
        double timestamp_s;  // wall-clock seconds since tracker reset
        double value;        // measured value (ms, %, Hz, …)
    };

    explicit PerformanceTracker(double window_s = 2.0, double ema_alpha = 0.05)
        : ema_alpha_(ema_alpha), window_s_(window_s) {
        reset();
    }

    void reset() {
        head_  = 0;
        count_ = 0;
        sum_   = 0.0;
        sum_sq_ = 0.0;
        ema_   = 0.0;
        ema_initialised_ = false;
    }

    /// Record a new sample.  Evicts samples outside the time window.
    void push(double timestamp_s, double value) {
        // Evict samples outside the sliding window
        evict_old(timestamp_s);

        // Write into the circular buffer
        size_t idx = (head_ + count_) % MaxSamples;
        if (count_ == MaxSamples) {
            // Buffer full — overwrite oldest, advance head
            sum_    -= samples_[head_].value;
            sum_sq_ -= samples_[head_].value * samples_[head_].value;
            head_ = (head_ + 1) % MaxSamples;
        } else {
            count_++;
        }
        samples_[idx] = {timestamp_s, value};
        sum_    += value;
        sum_sq_ += value * value;

        // EMA update
        if (!ema_initialised_) {
            ema_ = value;
            ema_initialised_ = true;
        } else {
            ema_ += ema_alpha_ * (value - ema_);
        }
    }

    // ---- Statistics (over samples currently in the window) ----

    [[nodiscard]] size_t count() const { return count_; }
    [[nodiscard]] bool   empty() const { return count_ == 0; }

    [[nodiscard]] double sum() const { return sum_; }

    [[nodiscard]] double average() const {
        return count_ > 0 ? sum_ / static_cast<double>(count_) : 0.0;
    }

    [[nodiscard]] double variance() const {
        if (count_ < 2) return 0.0;
        double n   = static_cast<double>(count_);
        double var = (sum_sq_ - (sum_ * sum_) / n) / n;
        return var > 0.0 ? var : 0.0;  // clamp rounding errors
    }

    [[nodiscard]] double stddev() const {
        return std::sqrt(variance());
    }

    [[nodiscard]] double ema() const { return ema_; }

    /// Reciprocal rate: 1000 / average (useful for ms → Hz conversion).
    [[nodiscard]] double hz() const {
        double avg = average();
        return avg > 0.0 ? 1000.0 / avg : 0.0;
    }

    [[nodiscard]] double min_in_window() const {
        if (count_ == 0) return 0.0;
        double m = samples_[head_].value;
        for (size_t i = 1; i < count_; i++)
            m = std::min(m, samples_[(head_ + i) % MaxSamples].value);
        return m;
    }

    [[nodiscard]] double max_in_window() const {
        if (count_ == 0) return 0.0;
        double m = samples_[head_].value;
        for (size_t i = 1; i < count_; i++)
            m = std::max(m, samples_[(head_ + i) % MaxSamples].value);
        return m;
    }

    /// Most recent sample value (or 0 if empty).
    [[nodiscard]] double last() const {
        if (count_ == 0) return 0.0;
        return samples_[(head_ + count_ - 1) % MaxSamples].value;
    }

    /// Most recent sample timestamp (or 0 if empty).
    [[nodiscard]] double last_timestamp() const {
        if (count_ == 0) return 0.0;
        return samples_[(head_ + count_ - 1) % MaxSamples].timestamp_s;
    }

    // ---- Graph data access ----

    /// Copy the N most recent sample *values* into a float array suitable
    /// for ImGui::PlotLines().  Returns the number of values written.
    size_t copy_values(float* out, size_t max_count) const {
        size_t n = count_ < max_count ? count_ : max_count;
        size_t start = (count_ > max_count) ? (count_ - max_count) : 0;
        for (size_t i = 0; i < n; i++) {
            out[i] = static_cast<float>(
                samples_[(head_ + start + i) % MaxSamples].value);
        }
        return n;
    }

    /// Direct read-only access to sample at logical index [0 = oldest].
    [[nodiscard]] const Sample& at(size_t i) const {
        return samples_[(head_ + i) % MaxSamples];
    }

    // ---- Persistence (opt-in, off by default) ----

    /// Start logging every pushed sample to a CSV file.
    void start_csv_log(const char* filepath) {
        stop_csv_log();
        csv_file_.open(filepath, std::ios::out | std::ios::trunc);
        if (csv_file_.is_open()) {
            csv_file_ << "timestamp_s,value\n";
        }
    }

    /// Stop CSV logging.
    void stop_csv_log() {
        if (csv_file_.is_open()) csv_file_.close();
    }

    [[nodiscard]] bool is_logging() const { return csv_file_.is_open(); }

    // ---- Binary snapshot (for session persistence) ----

    /// Write all current samples to a binary stream.
    /// Format: [uint32 count][Sample × count] (oldest first).
    void write_snapshot(std::ostream& os) const {
        uint32_t n = static_cast<uint32_t>(count_);
        os.write(reinterpret_cast<const char*>(&n), sizeof(n));
        for (size_t i = 0; i < count_; i++) {
            const auto& s = samples_[(head_ + i) % MaxSamples];
            os.write(reinterpret_cast<const char*>(&s), sizeof(Sample));
        }
    }

    /// Load samples from a binary snapshot, replacing current contents.
    void read_snapshot(std::istream& is) {
        reset();
        uint32_t n = 0;
        is.read(reinterpret_cast<char*>(&n), sizeof(n));
        if (n > MaxSamples) n = static_cast<uint32_t>(MaxSamples);
        for (uint32_t i = 0; i < n; i++) {
            Sample s;
            is.read(reinterpret_cast<char*>(&s), sizeof(Sample));
            // Push without window eviction so we preserve full loaded data
            size_t idx = (head_ + count_) % MaxSamples;
            samples_[idx] = s;
            count_++;
            sum_    += s.value;
            sum_sq_ += s.value * s.value;
        }
        if (count_ > 0) {
            ema_ = samples_[(head_ + count_ - 1) % MaxSamples].value;
            ema_initialised_ = true;
        }
    }

private:
    /// Evict samples older than (timestamp - window_s_).
    void evict_old(double current_timestamp) {
        double cutoff = current_timestamp - window_s_;
        while (count_ > 0 && samples_[head_].timestamp_s < cutoff) {
            sum_    -= samples_[head_].value;
            sum_sq_ -= samples_[head_].value * samples_[head_].value;
            head_ = (head_ + 1) % MaxSamples;
            count_--;
        }
    }

    /// Log a sample to CSV if logging is active.
    void log_csv(double timestamp_s, double value) {
        if (csv_file_.is_open()) {
            csv_file_ << std::fixed << std::setprecision(6)
                       << timestamp_s << ',' << value << '\n';
        }
    }

    std::array<Sample, MaxSamples> samples_;
    size_t head_  = 0;
    size_t count_ = 0;

    double sum_    = 0.0;
    double sum_sq_ = 0.0;

    double ema_         = 0.0;
    double ema_alpha_   = 0.05;
    bool   ema_initialised_ = false;

    double window_s_ = 2.0;  // sliding window duration in seconds

    std::ofstream csv_file_;  // optional CSV log (off by default)
};
