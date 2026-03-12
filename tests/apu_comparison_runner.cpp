// =============================================================================
// NES APU Reference Comparison Harness
// =============================================================================
// Runs cermu's NES APU side-by-side with Blargg's Nes_Snd_Emu (game-music-emu)
// as the reference oracle. Both APUs receive identical register writes and their
// audio output is compared sample-by-sample.
//
// Blargg's Nes_Snd_Emu generates audio via Blip_Buffer (band-limited synthesis),
// while cermu's APU uses direct sample() calls. To compare, we generate audio
// from both at the same sample rate and compare the waveform shape/timing.
//
// Usage:
//   apu_comparison_runner                 # Run all built-in comparison tests
//   apu_comparison_runner --verbose       # Show per-sample detail
//
// Exit code: 0 = all match, non-zero = number of failed tests
// =============================================================================

#include "../src/testing/apu_test_harness.hpp"

// Blargg's Nes_Snd_Emu
#include "Nes_Apu.h"
#include "Blip_Buffer.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>

// =============================================================================
// Constants
// =============================================================================

static constexpr int SAMPLE_RATE = 44100;
static constexpr int CPU_FREQ = 1789773; // NTSC
static constexpr int CYCLES_PER_SAMPLE = CPU_FREQ / SAMPLE_RATE; // ~40

// =============================================================================
// DMC reader stub (returns 0 — no sample memory for basic tests)
// =============================================================================

static int dmc_reader_stub(void*, nes_addr_t) {
    return 0;
}

// =============================================================================
// Dual-APU comparison engine
// =============================================================================

struct comparison_stats_t {
    uint64_t total_samples     = 0;
    uint64_t shape_mismatches  = 0; // samples where waveform direction disagrees
    uint64_t first_shape_mismatch = UINT64_MAX;
    double   max_amplitude_error = 0.0;
    double   sum_sq_error       = 0.0;
    bool     verbose            = false;

    void print_summary(const char* test_name) const {
        printf("\n── %s ──\n", test_name);
        printf("  Total samples compared: %llu\n", (unsigned long long)total_samples);
        printf("  Shape mismatches:       %llu\n", (unsigned long long)shape_mismatches);
        printf("  Max amplitude error:    %.6f\n", max_amplitude_error);
        printf("  RMS error:              %.6f\n",
               total_samples > 0 ? sqrt(sum_sq_error / total_samples) : 0.0);
        if (first_shape_mismatch < UINT64_MAX)
            printf("  First shape mismatch:   sample %llu\n", (unsigned long long)first_shape_mismatch);

        // Pass criteria: focus on waveform shape agreement
        // Note: amplitude comparison is inherently imprecise because
        // cermu uses direct DAC sampling while Blargg uses band-limited
        // delta synthesis (Blip_Buffer). DC offsets (e.g. triangle at
        // position 0) create constant amplitude differences.
        // Shape mismatches (opposite waveform direction) indicate real bugs.
        double rms = total_samples > 0 ? sqrt(sum_sq_error / total_samples) : 0.0;
        double shape_pct = total_samples > 0
            ? 100.0 * shape_mismatches / total_samples : 0.0;
        bool pass = (shape_pct < 2.0) && (rms < 0.40);
        printf("  Shape mismatch rate:    %.2f%%\n", shape_pct);
        printf("  Result:                 %s\n", pass ? "PASS" : "FAIL");
    }

    bool passed() const {
        double shape_pct = total_samples > 0
            ? 100.0 * shape_mismatches / total_samples : 0.0;
        double rms = total_samples > 0 ? sqrt(sum_sq_error / total_samples) : 0.0;
        return (shape_pct < 2.0) && (rms < 0.40);
    }
};

class DualAPU {
public:
    DualAPU() {
        // Initialize cermu APU
        harness_ = apu_test::create();

        // Initialize Blargg's APU
        // Use a bigger buffer to avoid overflow on long test runs
        buf_.sample_rate(SAMPLE_RATE);
        buf_.clock_rate(CPU_FREQ);
        ref_apu_.output(&buf_);
        ref_apu_.dmc_reader(dmc_reader_stub);
        ref_apu_.reset(false, 0);
    }

    ~DualAPU() {
        if (harness_) apu_test::destroy(harness_);
    }

    void reset() {
        apu_test::reset(harness_);
        ref_apu_.reset(false, 0);
        buf_.clear();
        time_ = 0;
    }

    void write(uint16_t addr, uint8_t value) {
        // Write to cermu APU
        apu_test::write_reg(harness_, addr, value);

        // Write to Blargg APU at current time
        ref_apu_.write_register(time_, addr, value);
    }

    // Clock both APUs for 'cycles' CPU cycles
    void clock(uint32_t cycles) {
        // Clock cermu APU cycle by cycle
        apu_test::clock_cycles(harness_, cycles);
        time_ += cycles;
    }

    // Generate audio samples from both and compare
    // Process in chunks to avoid Blip_Buffer overflow
    void compare_audio(uint32_t duration_cycles, comparison_stats_t& stats) {
        // Process in frames of ~1000 samples max to avoid Blip_Buffer overflow
        static constexpr int MAX_FRAME_SAMPLES = 1000;
        static constexpr int MAX_FRAME_CYCLES = MAX_FRAME_SAMPLES * CYCLES_PER_SAMPLE;

        uint32_t total_done = 0;
        while (total_done < duration_cycles) {
            uint32_t frame_cycles = std::min((uint32_t)MAX_FRAME_CYCLES,
                                             duration_cycles - total_done);

            // Collect cermu samples for this frame
            uint32_t cycles_in_frame = 0;
            while (cycles_in_frame < frame_cycles) {
                uint32_t chunk = CYCLES_PER_SAMPLE;
                if (cycles_in_frame + chunk > frame_cycles)
                    chunk = frame_cycles - cycles_in_frame;

                apu_test::clock_cycles(harness_, chunk);
                float our_sample = apu_test::get_sample(harness_);
                our_samples_.push_back(our_sample);

                cycles_in_frame += chunk;
            }

            time_ += frame_cycles;

            // End frame in Blargg and read reference samples
            ref_apu_.end_frame(frame_cycles);
            buf_.end_frame(frame_cycles);

            int ref_count = buf_.samples_avail();
            std::vector<short> ref_raw(ref_count);
            if (ref_count > 0)
                buf_.read_samples(ref_raw.data(), ref_count);

            // Compare this frame
            int compare_count = std::min((int)our_samples_.size(), ref_count);

            if (stats.verbose && stats.total_samples == 0) {
                printf("  Cermu samples: %d, Reference samples: %d, comparing: %d\n",
                       (int)our_samples_.size(), ref_count, compare_count);
            }

            for (int i = 0; i < compare_count; i++) {
                float our_val = our_samples_[i];
                float ref_val = ref_raw[i] / 32768.0f;

                float error = std::fabs(our_val - ref_val);
                stats.sum_sq_error += error * error;

                if (error > stats.max_amplitude_error) {
                    stats.max_amplitude_error = error;
                }

                // Shape mismatch: waveform going opposite direction
                if (stats.total_samples + i > 0 && i > 0) {
                    float our_delta = our_val - our_samples_[i-1];
                    float ref_delta = ref_val - (ref_raw[i-1] / 32768.0f);
                    if ((our_delta > 0.02f && ref_delta < -0.02f) ||
                        (our_delta < -0.02f && ref_delta > 0.02f)) {
                        stats.shape_mismatches++;
                        if (stats.first_shape_mismatch == UINT64_MAX)
                            stats.first_shape_mismatch = stats.total_samples + i;
                    }
                }

                if (stats.verbose && (stats.total_samples + i) < 32) {
                    printf("  [%4llu] ours=%.4f ref=%.4f err=%.4f\n",
                           (unsigned long long)(stats.total_samples + i),
                           our_val, ref_val, error);
                }
            }

            stats.total_samples += compare_count;
            our_samples_.clear();
            total_done += frame_cycles;
        }
    }

    // Get raw channel outputs from cermu (for per-channel comparison)
    uint8_t get_pulse1_output() { return apu_test::get_pulse1_output(harness_); }
    uint8_t get_pulse2_output() { return apu_test::get_pulse2_output(harness_); }
    uint8_t get_triangle_output() { return apu_test::get_triangle_output(harness_); }
    uint8_t get_noise_output() { return apu_test::get_noise_output(harness_); }

    // Direct access for inspection
    apu_test::harness_t* harness() { return harness_; }
    Nes_Apu& ref_apu() { return ref_apu_; }

    nes_time_t time() const { return time_; }

private:
    apu_test::harness_t* harness_ = nullptr;
    Nes_Apu              ref_apu_;
    Blip_Buffer          buf_;
    nes_time_t           time_ = 0;
    std::vector<float>   our_samples_;
};

// =============================================================================
// Built-in comparison tests
// =============================================================================

static int test_pulse_square_wave(DualAPU& dual, bool verbose) {
    printf("\n── Pulse square wave comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Set pulse 1: 50% duty, constant volume 15, medium frequency
    // $4000 = DDLC VVVV = 10 1 1 1111 = 0xBF (50% duty, halt, const, vol 15)
    dual.write(0x4000, 0xBF);
    // $4001 = sweep disabled
    dual.write(0x4001, 0x00);
    // $4002/$4003 = timer = 0x0C0 → period ~397 Hz
    dual.write(0x4002, 0xC0);
    // Enable channel
    dual.write(0x4015, 0x01);
    // $4003 = length counter load + timer high
    dual.write(0x4003, 0x08); // Length = some value, timer high = 0

    // Clock for ~0.1 seconds
    uint32_t cycles = CPU_FREQ / 10;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Pulse 50% duty");
    return stats.passed() ? 0 : 1;
}

static int test_pulse_25_duty(DualAPU& dual, bool verbose) {
    printf("\n── Pulse 25%% duty comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // 25% duty, constant volume 15
    dual.write(0x4000, 0x7F); // 01 DDLC VVVV = 01 1 1 1111
    dual.write(0x4001, 0x00);
    dual.write(0x4002, 0x80);
    dual.write(0x4015, 0x01);
    dual.write(0x4003, 0x08);

    uint32_t cycles = CPU_FREQ / 10;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Pulse 25% duty");
    return stats.passed() ? 0 : 1;
}

static int test_triangle_wave(DualAPU& dual, bool verbose) {
    printf("\n── Triangle wave comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Triangle: linear counter = 127, control flag = 1
    dual.write(0x4008, 0xFF); // Control=1, linear counter = 127
    dual.write(0x400A, 0xC0); // Timer low
    dual.write(0x4015, 0x04); // Enable triangle
    dual.write(0x400B, 0x08); // Timer high + length load

    uint32_t cycles = CPU_FREQ / 10;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Triangle wave");
    return stats.passed() ? 0 : 1;
}

static int test_noise_long(DualAPU& dual, bool verbose) {
    printf("\n── Noise long mode comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Noise: constant volume 15, period index 4
    dual.write(0x400C, 0x3F); // Halt, constant vol 15
    dual.write(0x400E, 0x04); // Long mode, period index 4
    dual.write(0x4015, 0x08); // Enable noise
    dual.write(0x400F, 0x08); // Length load

    uint32_t cycles = CPU_FREQ / 10;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Noise long mode");
    return stats.passed() ? 0 : 1;
}

static int test_noise_short(DualAPU& dual, bool verbose) {
    printf("\n── Noise short mode comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Noise: constant volume 15, short mode, period index 4
    dual.write(0x400C, 0x3F);
    dual.write(0x400E, 0x84); // Short mode + period index 4
    dual.write(0x4015, 0x08);
    dual.write(0x400F, 0x08);

    uint32_t cycles = CPU_FREQ / 10;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Noise short mode");
    return stats.passed() ? 0 : 1;
}

static int test_two_pulse_channels(DualAPU& dual, bool verbose) {
    printf("\n── Two pulse channels comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Pulse 1: 50% duty, vol 15, ~440 Hz (A4)
    // Period = CPU_FREQ / (16 * freq) - 1 = 1789773 / (16 * 440) - 1 ≈ 253
    dual.write(0x4000, 0xBF);
    dual.write(0x4001, 0x00);
    dual.write(0x4002, 0xFD); // Timer low (253 & 0xFF)
    dual.write(0x4015, 0x03); // Enable both pulses
    dual.write(0x4003, 0x08); // Timer high + length

    // Pulse 2: 25% duty, vol 10, ~554 Hz (C#5)
    // Period ≈ 201
    dual.write(0x4004, 0x7A); // 25% duty, constant vol 10
    dual.write(0x4005, 0x00);
    dual.write(0x4006, 0xC9); // Timer low (201 & 0xFF)
    dual.write(0x4007, 0x08); // Timer high + length

    uint32_t cycles = CPU_FREQ / 10;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Two pulse channels mixed");
    return stats.passed() ? 0 : 1;
}

static int test_envelope_decay(DualAPU& dual, bool verbose) {
    printf("\n── Envelope decay comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Pulse 1: envelope mode (not constant), period = 2
    dual.write(0x4000, 0x22); // Not halt, not constant, period=2
    dual.write(0x4001, 0x00);
    dual.write(0x4002, 0x80);
    dual.write(0x4015, 0x01);
    dual.write(0x4003, 0xF8); // Long length + timer high

    // Run for ~0.5 seconds to hear envelope decay
    uint32_t cycles = CPU_FREQ / 2;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Envelope decay");
    return stats.passed() ? 0 : 1;
}

static int test_sweep_up(DualAPU& dual, bool verbose) {
    printf("\n── Sweep up comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Pulse 1: 50% duty, constant vol 15, sweep up
    dual.write(0x4000, 0xBF);
    dual.write(0x4001, 0x85); // Sweep enabled, period=0, shift=5, negate=0
    dual.write(0x4002, 0x00); // Low period
    dual.write(0x4015, 0x01);
    dual.write(0x4003, 0xF1); // High period + length

    uint32_t cycles = CPU_FREQ / 5;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Sweep up");
    return stats.passed() ? 0 : 1;
}

static int test_all_channels(DualAPU& dual, bool verbose) {
    printf("\n── All channels mixed comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Enable all standard channels
    dual.write(0x4015, 0x0F);

    // Pulse 1: 50% duty, vol 15, ~262 Hz (C4)
    dual.write(0x4000, 0xBF);
    dual.write(0x4001, 0x00);
    dual.write(0x4002, 0xAB); // Period ~425
    dual.write(0x4003, 0x09);

    // Pulse 2: 12.5% duty, vol 10, ~330 Hz (E4)
    dual.write(0x4004, 0x1A);
    dual.write(0x4005, 0x00);
    dual.write(0x4006, 0x56); // Period ~338
    dual.write(0x4007, 0x09);

    // Triangle: ~196 Hz (G3)
    dual.write(0x4008, 0xFF);
    dual.write(0x400A, 0x8E); // Period ~568
    dual.write(0x400B, 0x0A);

    // Noise: medium period, long mode
    dual.write(0x400C, 0x3C); // Halt, constant vol 12
    dual.write(0x400E, 0x06); // Period index 6
    dual.write(0x400F, 0x08);

    uint32_t cycles = CPU_FREQ / 10;
    dual.compare_audio(cycles, stats);

    stats.print_summary("All channels mixed");
    return stats.passed() ? 0 : 1;
}

// =============================================================================
// Waveform dump comparison (for debugging)
// =============================================================================

static int test_pulse_waveform_dump(DualAPU& dual, bool verbose) {
    printf("\n── Pulse waveform dump (first 200 samples) ──\n");
    comparison_stats_t stats;
    stats.verbose = true; // Always verbose for dump
    dual.reset();

    // Simple pulse for waveform inspection
    dual.write(0x4000, 0xBF); // 50% duty, constant vol 15
    dual.write(0x4001, 0x00);
    dual.write(0x4002, 0x20); // Short period for visible waveform
    dual.write(0x4015, 0x01);
    dual.write(0x4003, 0x08);

    // Just 200 samples worth
    uint32_t cycles = CYCLES_PER_SAMPLE * 200;
    dual.compare_audio(cycles, stats);

    stats.print_summary("Pulse waveform dump");
    return stats.passed() ? 0 : 1;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
            verbose = true;
    }

    printf("\n══════════════════════════════════════════════════\n");
    printf("  NES APU Reference Comparison (cermu vs Blargg)\n");
    printf("══════════════════════════════════════════════════\n");

    DualAPU dual;
    int total_failures = 0;

    total_failures += test_pulse_square_wave(dual, verbose);
    total_failures += test_pulse_25_duty(dual, verbose);
    total_failures += test_triangle_wave(dual, verbose);
    total_failures += test_noise_long(dual, verbose);
    total_failures += test_noise_short(dual, verbose);
    total_failures += test_two_pulse_channels(dual, verbose);
    total_failures += test_envelope_decay(dual, verbose);
    total_failures += test_sweep_up(dual, verbose);
    total_failures += test_all_channels(dual, verbose);

    if (verbose) {
        total_failures += test_pulse_waveform_dump(dual, verbose);
    }

    printf("\n══════════════════════════════════════════════════\n");
    if (total_failures == 0) {
        printf("  ALL COMPARISON TESTS PASSED\n");
    } else {
        printf("  %d COMPARISON TEST(S) FAILED\n", total_failures);
    }
    printf("══════════════════════════════════════════════════\n\n");

    return total_failures;
}
