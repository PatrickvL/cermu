#pragma once
/**
 * performance_metrics.hpp — System-level performance metrics aggregator
 *
 * Owns multiple PerformanceTracker instances and scalar counters, providing
 * a single point of access for all emulator performance data.
 *
 * Designed to live on the EmulatorHost (one instance, shared across system
 * switches).  Reset when a new system is loaded.
 *
 * Thread safety: the trackers are NOT thread-safe internally.  The emu
 * thread pushes samples (under emu_mutex_ or from the single emu thread),
 * and the GUI thread reads for rendering.  Audio counters use atomics
 * since the SDL audio callback runs on a separate thread.
 */

#include <atomic>
#include <cstdint>
#include <fstream>
#include "utils/performance_tracker.hpp"

struct PerformanceMetrics {
    // ---- Time-series trackers (pushed per-frame by the emu thread) ----
    PerformanceTracker<2048> frame_time{2.0, 0.05};      // run_frame() wall time (ms)
    PerformanceTracker<2048> frame_interval{2.0, 0.05};   // time between frames (ms)
    PerformanceTracker<1024> frame_time_long{15.0, 0.002}; // run_frame() wall time, 15s window (for headroom bar)
    PerformanceTracker<512>  audio_buffer_fill{4.0, 0.1};  // host ring buffer fill % (0–100)
    PerformanceTracker<512>  audio_gen_time{2.0, 0.1};     // get_audio_samples() wall time (µs)

    // ---- Scalar speed metrics (updated per-frame by the emu thread) ----
    double speed_percent     = 0.0;   // emulated time / wall time × 100
    double max_speed_percent = 0.0;   // target_frame_time / frame_emu_time × 100 (headroom)
    double target_fps        = 0.0;   // real hardware refresh rate (e.g. 50.12 PAL, 59.94 NTSC)

    // ---- Audio health (atomics — SDL callback thread writes, GUI reads) ----
    std::atomic<uint32_t> audio_underruns{0};  // ring empty when SDL wanted samples
    std::atomic<uint32_t> audio_overruns{0};   // ring full when emu tried to write
    std::atomic<uint32_t> audio_samples_total{0};  // total samples produced this session

    // ---- Totals ----
    uint64_t total_frames = 0;
    double   uptime_s     = 0.0;  // wall-clock seconds since reset

    // ---- Reference clock ----
    double start_timestamp_s = 0.0;  // SDL perf counter at reset, in seconds

    void reset() {
        frame_time.reset();
        frame_interval.reset();
        frame_time_long.reset();
        audio_buffer_fill.reset();
        audio_gen_time.reset();

        speed_percent     = 0.0;
        max_speed_percent = 0.0;
        target_fps        = 0.0;

        audio_underruns.store(0, std::memory_order_relaxed);
        audio_overruns.store(0, std::memory_order_relaxed);
        audio_samples_total.store(0, std::memory_order_relaxed);

        total_frames      = 0;
        uptime_s          = 0.0;
        start_timestamp_s = 0.0;
    }

    // ---- CSV logging control (delegates to all trackers) ----

    void start_csv_log(const char* base_path) {
        std::string base(base_path);
        frame_time.start_csv_log((base + "_frame_time.csv").c_str());
        frame_interval.start_csv_log((base + "_frame_interval.csv").c_str());
        audio_buffer_fill.start_csv_log((base + "_audio_fill.csv").c_str());
        audio_gen_time.start_csv_log((base + "_audio_gen_time.csv").c_str());
    }

    void stop_csv_log() {
        frame_time.stop_csv_log();
        frame_interval.stop_csv_log();
        audio_buffer_fill.stop_csv_log();
        audio_gen_time.stop_csv_log();
    }

    [[nodiscard]] bool is_logging() const {
        return frame_time.is_logging();
    }

    // ---- Binary session snapshot ----

    void save_snapshot(const char* filepath) const {
        std::ofstream os(filepath, std::ios::binary);
        if (!os.is_open()) return;
        frame_time.write_snapshot(os);
        frame_interval.write_snapshot(os);
        frame_time_long.write_snapshot(os);
        audio_buffer_fill.write_snapshot(os);
        audio_gen_time.write_snapshot(os);
        // Scalar state
        os.write(reinterpret_cast<const char*>(&speed_percent), sizeof(speed_percent));
        os.write(reinterpret_cast<const char*>(&max_speed_percent), sizeof(max_speed_percent));
        os.write(reinterpret_cast<const char*>(&target_fps), sizeof(target_fps));
        uint32_t ur = audio_underruns.load(std::memory_order_relaxed);
        uint32_t or_ = audio_overruns.load(std::memory_order_relaxed);
        os.write(reinterpret_cast<const char*>(&ur), sizeof(ur));
        os.write(reinterpret_cast<const char*>(&or_), sizeof(or_));
        os.write(reinterpret_cast<const char*>(&total_frames), sizeof(total_frames));
        os.write(reinterpret_cast<const char*>(&uptime_s), sizeof(uptime_s));
    }

    void load_snapshot(const char* filepath) {
        std::ifstream is(filepath, std::ios::binary);
        if (!is.is_open()) return;
        frame_time.read_snapshot(is);
        frame_interval.read_snapshot(is);
        frame_time_long.read_snapshot(is);
        audio_buffer_fill.read_snapshot(is);
        audio_gen_time.read_snapshot(is);
        is.read(reinterpret_cast<char*>(&speed_percent), sizeof(speed_percent));
        is.read(reinterpret_cast<char*>(&max_speed_percent), sizeof(max_speed_percent));
        is.read(reinterpret_cast<char*>(&target_fps), sizeof(target_fps));
        uint32_t ur = 0, or_ = 0;
        is.read(reinterpret_cast<char*>(&ur), sizeof(ur));
        is.read(reinterpret_cast<char*>(&or_), sizeof(or_));
        audio_underruns.store(ur, std::memory_order_relaxed);
        audio_overruns.store(or_, std::memory_order_relaxed);
        is.read(reinterpret_cast<char*>(&total_frames), sizeof(total_frames));
        is.read(reinterpret_cast<char*>(&uptime_s), sizeof(uptime_s));
    }
};
