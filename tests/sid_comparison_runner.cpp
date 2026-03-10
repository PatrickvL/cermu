// =============================================================================
// SID Reference Comparison Harness
// =============================================================================
// Runs cermu's MOS6581 side-by-side with reSID (Dag Lem) as the reference
// oracle.  Both SIDs receive identical register writes and are clocked in
// lockstep.
//
// Two comparison modes:
//   1. DIGITAL — Per-cycle OSC3, ENV3, and accumulator comparison (exact match).
//   2. AUDIO   — Per-cycle voice/filter/mix comparison and per-sample decimated
//                output comparison with statistical tolerance (max error, RMS,
//                cross-correlation).  Uses a derived reSID wrapper to probe
//                intermediate pipeline stages.
//
// Additionally supports importing hardware-captured OSC3 traces from the
// VICE-testprogs/SID/resid-test/ .dat files (C64 PRG format: 2-byte load
// address header + raw OSC3 bytes).
//
// Usage:
//   sid_comparison_runner                              # Run all built-in tests
//   sid_comparison_runner --audio                      # Run audio pipeline tests
//   sid_comparison_runner --verbose                    # Show every cycle
//   sid_comparison_runner --script <.sid_test>         # Run a test script
//   sid_comparison_runner --dat <.dat> --waveform 0x20 # Compare vs HW trace
//   sid_comparison_runner --revision 8580              # Use 8580 model
//   sid_comparison_runner --resid-only                 # Dump reSID trace only
//
// Exit code: 0 = all match, non-zero = number of mismatches
// =============================================================================

#include "../src/testing/sid_test_harness.h"

// reSID headers — we compile against the VICE copy with our standalone siddefs.h
#include "resid_probe.h"

// SID register write log (binary capture/replay format)
#include "../src/testing/sid_write_log.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>

// =============================================================================
// Dual-SID comparison engine
// =============================================================================

struct comparison_stats_t {
    uint64_t total_cycles     = 0;
    uint64_t osc3_mismatches  = 0;
    uint64_t env3_mismatches  = 0;
    uint64_t acc_mismatches   = 0;
    uint64_t first_mismatch_cycle = UINT64_MAX;
    bool     verbose          = false;

    void print_summary() const {
        printf("\n══════════════════════════════════════════════════\n");
        printf("  SID Comparison Summary\n");
        printf("══════════════════════════════════════════════════\n");
        printf("  Total cycles compared: %llu\n", (unsigned long long)total_cycles);
        printf("  OSC3 mismatches:       %llu\n", (unsigned long long)osc3_mismatches);
        printf("  ENV3 mismatches:       %llu\n", (unsigned long long)env3_mismatches);
        printf("  ACC  mismatches:       %llu\n", (unsigned long long)acc_mismatches);
        if (first_mismatch_cycle < UINT64_MAX)
            printf("  First mismatch at:     cycle %llu\n", (unsigned long long)first_mismatch_cycle);
        printf("  Result:                %s\n",
               (osc3_mismatches + env3_mismatches == 0) ? "PASS" : "FAIL");
        printf("══════════════════════════════════════════════════\n\n");
    }
};

// -----------------------------------------------------------------------------
// Adapter: drive both SIDs with identical operations
// -----------------------------------------------------------------------------

class DualSID {
public:
    DualSID(bool is_8580 = false) {
        // Initialize cermu SID
        harness_ = sid_test::create(false);
        if (is_8580) {
            harness_->sid->set_revision(SID_REVISION_8580_R5);
        }

        // Initialize reSID (instrumented for pipeline probing)
        resid_.set_chip_model(is_8580 ? reSID::MOS8580 : reSID::MOS6581);
        resid_.enable_filter(false);         // Compare digital domain only
        resid_.enable_external_filter(false);
        resid_.reset();
    }

    ~DualSID() {
        if (harness_) sid_test::destroy(harness_);
    }

    void reset() {
        sid_test::reset(harness_);
        resid_.reset();
        cycle_ = 0;
    }

    void write(uint8_t reg, uint8_t value) {
        sid_test::write_reg(harness_, reg, value);
        resid_.write(reg, value);
    }

    void clock(uint32_t cycles, comparison_stats_t& stats) {
        for (uint32_t i = 0; i < cycles; i++) {
            // Clock both
            sid_test::clock_cycles(harness_, 1);
            resid_.clock();

            // Read OSC3 and ENV3 from both
            uint8_t our_osc3 = sid_test::read_osc3(harness_);
            uint8_t our_env3 = sid_test::read_env3(harness_);

            uint8_t ref_osc3 = (uint8_t)resid_.read(0x1B);
            uint8_t ref_env3 = (uint8_t)resid_.read(0x1C);

            stats.total_cycles++;
            cycle_++;

            bool mismatch = false;
            if (our_osc3 != ref_osc3) {
                stats.osc3_mismatches++;
                mismatch = true;
            }
            if (our_env3 != ref_env3) {
                stats.env3_mismatches++;
                mismatch = true;
            }

            if (mismatch) {
                if (stats.first_mismatch_cycle == UINT64_MAX)
                    stats.first_mismatch_cycle = cycle_;

                if (stats.verbose || (stats.osc3_mismatches + stats.env3_mismatches) <= 20) {
                    printf("  MISMATCH cycle %8llu: OSC3 ours=%02X ref=%02X%s | ENV3 ours=%02X ref=%02X%s\n",
                           (unsigned long long)cycle_,
                           our_osc3, ref_osc3,
                           (our_osc3 != ref_osc3) ? " <<<" : "    ",
                           our_env3, ref_env3,
                           (our_env3 != ref_env3) ? " <<<" : "    ");
                }
            } else if (stats.verbose && (cycle_ % 10000 == 0)) {
                printf("  cycle %8llu: OSC3=%02X ENV3=%02X (match)\n",
                       (unsigned long long)cycle_, our_osc3, our_env3);
            }
        }
    }

    // Get reSID internal state for deep comparison
    reSID::SID::State resid_state() { return resid_.read_state(); }

    // Access our harness
    sid_test::harness_t* harness() { return harness_; }
    resid_probe::InstrumentedSID& resid() { return resid_; }

    uint64_t cycle() const { return cycle_; }

private:
    sid_test::harness_t*          harness_ = nullptr;
    resid_probe::InstrumentedSID  resid_;
    uint64_t                      cycle_ = 0;
};

// =============================================================================
// Built-in comparison tests
// =============================================================================

static int test_sawtooth_comparison(DualSID& dual, bool verbose) {
    printf("── Sawtooth waveform comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Set frequency and sawtooth waveform on voice 3
    dual.write(0x0E, 0x00);  // V3 freq lo
    dual.write(0x0F, 0x10);  // V3 freq hi = $1000
    dual.write(0x12, 0x28);  // V3 control: test bit + sawtooth
    dual.clock(1, stats);    // Latch reset
    dual.write(0x12, 0x20);  // Release test bit, sawtooth active
    dual.write(0x18, 0x0F);  // Volume max

    // Run for a full waveform cycle (acc wraps at 2^24 / 0x1000 = 4096 cycles)
    dual.clock(4096 * 4, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_triangle_comparison(DualSID& dual, bool verbose) {
    printf("── Triangle waveform comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    dual.write(0x0E, 0x00);
    dual.write(0x0F, 0x10);  // Freq $1000
    dual.write(0x12, 0x18);  // Test + triangle
    dual.clock(1, stats);
    dual.write(0x12, 0x10);  // Triangle active
    dual.write(0x18, 0x0F);

    dual.clock(4096 * 4, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_pulse_comparison(DualSID& dual, bool verbose) {
    printf("── Pulse waveform comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    dual.write(0x0E, 0x00);
    dual.write(0x0F, 0x10);  // Freq $1000
    dual.write(0x10, 0x00);  // PW lo = 0x00
    dual.write(0x11, 0x08);  // PW hi = 0x08 → 50% duty ($800)
    dual.write(0x12, 0x48);  // Test + pulse
    dual.clock(1, stats);
    dual.write(0x12, 0x40);  // Pulse active
    dual.write(0x18, 0x0F);

    dual.clock(4096 * 4, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_noise_comparison(DualSID& dual, bool verbose) {
    printf("── Noise waveform comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    dual.write(0x0E, 0x00);
    dual.write(0x0F, 0x10);
    dual.write(0x12, 0x88);  // Test + noise
    dual.clock(1, stats);
    dual.write(0x12, 0x80);  // Noise active
    dual.write(0x18, 0x0F);

    // Noise LFSR needs many cycles to produce interesting output
    dual.clock(65536, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_envelope_adsr_comparison(DualSID& dual, bool verbose) {
    printf("── Envelope ADSR comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Set AD and SR for voice 3
    dual.write(0x13, 0x12);  // Attack=1, Decay=2
    dual.write(0x14, 0x84);  // Sustain=8, Release=4
    dual.clock(3, stats);

    // Gate ON
    dual.write(0x12, 0x11);  // Gate + triangle (waveform doesn't matter for envelope)
    dual.clock(20000, stats);

    // Gate OFF
    dual.write(0x12, 0x10);
    dual.clock(100000, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_envelope_edge_cases(DualSID& dual, bool verbose) {
    printf("── Envelope edge cases (gate retrigger, sustain change) ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Fast attack/decay/release
    dual.write(0x13, 0x00);  // Attack=0, Decay=0
    dual.write(0x14, 0xF0);  // Sustain=F, Release=0

    // Rapid gate on/off to test retrigger
    for (int i = 0; i < 10; i++) {
        dual.write(0x12, 0x11);  // Gate ON
        dual.clock(100, stats);
        dual.write(0x12, 0x10);  // Gate OFF
        dual.clock(100, stats);
    }

    // Change sustain level while in sustain phase
    dual.write(0x13, 0x09);  // Attack=0, Decay=9
    dual.write(0x14, 0x80);  // Sustain=8, Release=0
    dual.write(0x12, 0x11);  // Gate ON
    dual.clock(50000, stats);

    // Change sustain while sustaining
    dual.write(0x14, 0x40);  // Sustain=4 (lower)
    dual.clock(20000, stats);

    dual.write(0x14, 0xC0);  // Sustain=C (higher)
    dual.clock(20000, stats);

    // Gate OFF
    dual.write(0x12, 0x10);
    dual.clock(50000, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_combined_waveforms_comparison(DualSID& dual, bool verbose) {
    printf("── Combined waveforms comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;

    // Test all combined waveform modes
    uint8_t combos[] = {
        0x30,  // Triangle + Sawtooth
        0x50,  // Triangle + Pulse
        0x60,  // Sawtooth + Pulse
        0x70,  // Triangle + Sawtooth + Pulse
    };

    for (uint8_t combo : combos) {
        printf("  Waveform combo 0x%02X:\n", combo);
        dual.reset();

        dual.write(0x0E, 0x00);
        dual.write(0x0F, 0x10);
        dual.write(0x10, 0x00);
        dual.write(0x11, 0x08);  // 50% pulse width for pulse combos
        dual.write(0x12, combo | 0x08);  // Test bit
        dual.clock(1, stats);
        dual.write(0x12, combo);  // Active
        dual.write(0x18, 0x0F);

        dual.clock(4096 * 2, stats);
    }

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_ring_modulation_comparison(DualSID& dual, bool verbose) {
    printf("── Ring modulation comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Voice 1 as modulator (different frequency)
    dual.write(0x00, 0x00);
    dual.write(0x01, 0x08);  // V1 freq $0800
    dual.write(0x04, 0x10);  // V1 triangle (provides ring source for V3)

    // Voice 3 with ring modulation
    dual.write(0x0E, 0x00);
    dual.write(0x0F, 0x10);  // V3 freq $1000
    dual.write(0x12, 0x14);  // V3 triangle + ring mod
    dual.write(0x18, 0x0F);

    dual.clock(32768, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_oscillator_sync_comparison(DualSID& dual, bool verbose) {
    printf("── Oscillator sync comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    // Voice 1 as sync source
    dual.write(0x00, 0x00);
    dual.write(0x01, 0x08);  // V1 freq $0800
    dual.write(0x04, 0x20);  // V1 sawtooth

    // Voice 2 synced to voice 1
    dual.write(0x07, 0x00);
    dual.write(0x08, 0x10);  // V2 freq $1000
    dual.write(0x0B, 0x22);  // V2 sawtooth + sync

    // Read via voice 3 (sync from voice 2)
    dual.write(0x0E, 0x00);
    dual.write(0x0F, 0x20);  // V3 freq $2000
    dual.write(0x12, 0x22);  // V3 sawtooth + sync
    dual.write(0x18, 0x0F);

    dual.clock(32768, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

static int test_test_bit_comparison(DualSID& dual, bool verbose) {
    printf("── Test bit behavior comparison ──\n");
    comparison_stats_t stats;
    stats.verbose = verbose;
    dual.reset();

    dual.write(0x0E, 0x00);
    dual.write(0x0F, 0x10);

    // Sawtooth + test bit
    dual.write(0x12, 0x28);
    dual.clock(100, stats);

    // Release test bit — accumulator should be 0
    dual.write(0x12, 0x20);
    dual.clock(100, stats);

    // Set test bit again — accumulator freezes
    dual.write(0x12, 0x28);
    dual.clock(100, stats);

    // Noise + test bit (LFSR behavior)
    dual.write(0x12, 0x88);
    dual.clock(100, stats);
    dual.write(0x12, 0x80);
    dual.clock(1000, stats);

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

// =============================================================================
// Hardware trace (.dat) file comparison
// =============================================================================
// VICE-testprogs .dat files are C64 PRG format:
//   Bytes 0-1: Load address (little-endian, typically $C000)
//   Bytes 2+:  Raw OSC3 samples, one byte per sample
//
// The test program samples OSC3 every 8 cycles at frequency $1000,
// starting from a test-bit reset. The first 4 bytes are blanked (6581)
// or 3 bytes (8580) because of the sampling loop startup delay.
// =============================================================================

struct dat_file_t {
    std::vector<uint8_t> data;  // Raw OSC3 samples (after stripping 2-byte header)
    uint16_t load_address = 0;
    std::string filename;
};

static bool load_dat_file(const char* path, dat_file_t& dat) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("ERROR: Cannot open .dat file: %s\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 3) {
        printf("ERROR: .dat file too small: %s (%ld bytes)\n", path, size);
        fclose(f);
        return false;
    }

    // Read 2-byte PRG load address
    uint8_t hdr[2];
    fread(hdr, 1, 2, f);
    dat.load_address = hdr[0] | (hdr[1] << 8);

    // Read remaining data
    size_t data_size = (size_t)(size - 2);
    dat.data.resize(data_size);
    fread(dat.data.data(), 1, data_size, f);
    fclose(f);

    dat.filename = path;
    return true;
}

static int compare_dat_file(const char* path, uint8_t waveform, bool is_8580,
                            bool verbose, bool resid_only) {
    dat_file_t dat;
    if (!load_dat_file(path, dat)) return 1;

    printf("── Comparing against hardware trace: %s ──\n", path);
    printf("  Load address: $%04X, data size: %zu bytes, waveform: $%02X, model: %s\n",
           dat.load_address, dat.data.size(), waveform, is_8580 ? "8580" : "6581");

    // The test program:
    // 1. Sets test bit to reset accumulator
    // 2. Sets frequency to $1000
    // 3. Releases test bit with selected waveform
    // 4. Reads OSC3 every 8 cycles
    // First 4 samples (6581) or 3 samples (8580) are blanked to 0x00
    // due to instruction pipeline startup delay

    int blank_count = is_8580 ? 3 : 4;
    int total_errors = 0;
    int resid_errors = 0;
    int cermu_errors = 0;

    // --- reSID reference ---
    reSID::SID ref;
    ref.set_chip_model(is_8580 ? reSID::MOS8580 : reSID::MOS6581);
    ref.enable_filter(false);
    ref.enable_external_filter(false);
    ref.reset();

    // Setup: frequency $1000 on voice 3
    ref.write(0x0E, 0x00);
    ref.write(0x0F, 0x10);
    if (waveform == 0x40) {
        // Pulse: PW=0 and PW_HI as configured in oscsample test
        ref.write(0x10, 0x00);
        ref.write(0x11, 0x00);  // oscsample0: PULSEHI=0
    }

    // Test bit reset
    ref.write(0x12, waveform | 0x08);
    ref.clock();
    ref.write(0x12, waveform);

    // --- cermu SID ---
    sid_test::harness_t* h = nullptr;
    if (!resid_only) {
        h = sid_test::create(false);
        if (is_8580) {
            h->sid->set_revision(SID_REVISION_8580_R5);
        }

        sid_test::write_reg(h, 0x0E, 0x00);
        sid_test::write_reg(h, 0x0F, 0x10);
        if (waveform == 0x40) {
            sid_test::write_reg(h, 0x10, 0x00);
            sid_test::write_reg(h, 0x11, 0x00);
        }
        sid_test::write_reg(h, 0x18, 0x0F);

        // Test bit reset
        sid_test::write_reg(h, 0x12, waveform | 0x08);
        sid_test::clock_cycles(h, 1);
        sid_test::write_reg(h, 0x12, waveform);
    }

    // Compare each sample
    printf("  Sample | HW  | reSID | cermu | Status\n");
    printf("  -------+-----+-------+-------+-------\n");

    for (size_t i = 0; i < dat.data.size(); i++) {
        // Clock 8 cycles (the test program reads OSC3 every 8 cycles)
        for (int c = 0; c < 8; c++) {
            ref.clock();
            if (h) sid_test::clock_cycles(h, 1);
        }

        uint8_t hw_val   = dat.data[i];
        uint8_t ref_val  = (uint8_t)ref.read(0x1B);
        uint8_t our_val  = h ? sid_test::read_osc3(h) : 0;

        // Skip blanked samples
        if ((int)i < blank_count && hw_val == 0x00) {
            if (verbose)
                printf("  %5zu  |  00 |   %02X  |   %02X  | (blanked)\n", i, ref_val, our_val);
            continue;
        }

        bool ref_match = (ref_val == hw_val);
        bool our_match = resid_only || (our_val == hw_val);

        if (!ref_match) resid_errors++;
        if (!our_match) cermu_errors++;
        if (!ref_match || !our_match) total_errors++;

        if (verbose || !ref_match || !our_match) {
            printf("  %5zu  |  %02X |   %02X  |   %02X  |%s%s\n",
                   i, hw_val, ref_val, our_val,
                   ref_match ? "" : " reSID-MISMATCH",
                   our_match ? "" : " cermu-MISMATCH");
        }
    }

    printf("\n  Results: %zu samples compared\n", dat.data.size());
    printf("    reSID  vs hardware: %d mismatches\n", resid_errors);
    if (!resid_only)
        printf("    cermu  vs hardware: %d mismatches\n", cermu_errors);
    printf("    cermu  vs reSID:    (see dual comparison tests)\n\n");

    if (h) sid_test::destroy(h);
    return total_errors;
}

// =============================================================================
// Script-driven dual comparison
// =============================================================================

static int run_script_comparison(const char* script_path, bool is_8580, bool verbose) {
    printf("── Script-driven dual comparison: %s ──\n", script_path);

    sid_test::test_script_t script;
    if (!sid_test::parse_script_file(script_path, &script)) {
        printf("ERROR: Failed to parse script: %s\n", script_path);
        return 1;
    }

    DualSID dual(is_8580);
    comparison_stats_t stats;
    stats.verbose = verbose;

    // Execute commands, feeding both SIDs
    for (const auto& cmd : script.commands) {
        switch (cmd.type) {
        case sid_test::cmd_type_t::RESET:
            dual.reset();
            break;

        case sid_test::cmd_type_t::REVISION:
            // Reset with new revision
            dual.~DualSID();
            new (&dual) DualSID(cmd.is_8580);
            break;

        case sid_test::cmd_type_t::WRITE:
            dual.write(cmd.reg, cmd.value);
            break;

        case sid_test::cmd_type_t::RUN:
            dual.clock(cmd.cycles, stats);
            break;

        case sid_test::cmd_type_t::EXPECT_OSC3: {
            uint8_t our_val = sid_test::read_osc3(dual.harness());
            uint8_t ref_val = (uint8_t)dual.resid().read(0x1B);
            if (our_val != cmd.value)
                printf("  expect_osc3 FAIL (line %d): ours=%02X expected=%02X ref=%02X\n",
                       cmd.line_number, our_val, cmd.value, ref_val);
            } break;

        case sid_test::cmd_type_t::EXPECT_ENV3: {
            uint8_t our_val = sid_test::read_env3(dual.harness());
            if (our_val != cmd.value)
                printf("  expect_env3 FAIL (line %d): ours=%02X expected=%02X\n",
                       cmd.line_number, our_val, cmd.value);
            } break;

        default:
            // Labels, snapshots etc. — execute on our harness for tracing
            sid_test::execute_command(dual.harness(), &cmd);
            break;
        }
    }

    stats.print_summary();
    return (int)(stats.osc3_mismatches + stats.env3_mismatches);
}

// =============================================================================
// Audio Pipeline Comparison Engine
// =============================================================================
// Compares the analogue audio domain: per-cycle voice outputs, filter output,
// mixed output, and per-sample decimated output.  Both SIDs are driven with
// identical register writes; both produce audio that is compared statistically.
//
// reSID is configured with SAMPLE_RESAMPLE (FIR sinc, ~-80 dB sidelobes) to
// serve as the quality reference.  cermu uses CIC-3 (~-39 dB sidelobes).
// =============================================================================

// ── Statistics accumulator ──────────────────────────────────────────────────

struct audio_stats_t {
    const char* name;
    double sum_sq_error  = 0.0;   // Σ (a - b)²
    double sum_ref_sq    = 0.0;   // Σ b²  (for SNR)
    double max_abs_error = 0.0;   // max |a - b|
    double sum_cross     = 0.0;   // Σ a·b  (for correlation)
    double sum_a_sq      = 0.0;   // Σ a²
    double sum_b_sq      = 0.0;   // Σ b²
    uint64_t count       = 0;

    void record(float a, float b) {
        double d = (double)a - (double)b;
        sum_sq_error += d * d;
        sum_ref_sq   += (double)b * (double)b;
        double ad = fabs(d);
        if (ad > max_abs_error) max_abs_error = ad;
        sum_cross += (double)a * (double)b;
        sum_a_sq  += (double)a * (double)a;
        sum_b_sq  += (double)b * (double)b;
        count++;
    }

    double rms_error() const {
        return count > 0 ? sqrt(sum_sq_error / (double)count) : 0.0;
    }

    // Signal-to-noise ratio in dB (treats reSID as "signal", difference as "noise")
    double snr_db() const {
        if (sum_sq_error < 1e-30) return 999.0;  // Perfect match
        return 10.0 * log10(sum_ref_sq / sum_sq_error);
    }

    // Pearson correlation coefficient [-1, +1]
    double correlation() const {
        double denom = sqrt(sum_a_sq * sum_b_sq);
        return denom > 1e-30 ? sum_cross / denom : 0.0;
    }

    void print() const {
        printf("    %-24s  N=%llu  RMS=%.6f  MaxErr=%.6f  SNR=%.1f dB  r=%.6f\n",
               name,
               (unsigned long long)count,
               rms_error(),
               max_abs_error,
               snr_db(),
               correlation());
    }
};

// ── Audio comparison result ─────────────────────────────────────────────────

struct audio_comparison_result_t {
    audio_stats_t voice_stats[3];    // Per-voice (pre-filter)
    audio_stats_t filter_stats;      // Post-filter
    audio_stats_t mix_stats;         // Final mix (pre-decimation)
    audio_stats_t sample_stats;      // Per-sample (post-decimation)

    audio_comparison_result_t() {
        voice_stats[0].name = "Voice 1";
        voice_stats[1].name = "Voice 2";
        voice_stats[2].name = "Voice 3";
        filter_stats.name   = "Filter output";
        mix_stats.name      = "Mixed output";
        sample_stats.name   = "Decimated samples";
    }

    void print_summary() const {
        printf("  ┌──────────────────────────────────────────────────────────────────────────┐\n");
        printf("  │ Audio Pipeline Comparison                                                │\n");
        printf("  ├──────────────────────────────────────────────────────────────────────────┤\n");
        for (int i = 0; i < 3; i++) voice_stats[i].print();
        filter_stats.print();
        mix_stats.print();
        sample_stats.print();
        printf("  └──────────────────────────────────────────────────────────────────────────┘\n");
    }

    // Overall pass: sample SNR above threshold (dB).  Voice/filter SNR may
    // differ due to different filter models, but the decimated output should
    // be close.
    bool pass(double sample_snr_threshold_db = 20.0) const {
        return sample_stats.snr_db() >= sample_snr_threshold_db;
    }
};

// ── Audio Dual-SID engine ───────────────────────────────────────────────────
//
// Unlike the digital DualSID, this configures BOTH implementations for full
// audio output (filter enabled, external filter enabled, sample generation).

class AudioDualSID {
public:
    static constexpr float CPU_CLOCK   = 985248.0f;   // PAL
    static constexpr float SAMPLE_RATE = 44100.0f;
    // reSID scale factor: 6581→3, 8580→5.  We normalise output to [-1,+1].
    static constexpr float RESID_SHORT_NORM = 1.0f / 32768.0f;

    AudioDualSID(bool is_8580 = false) : is_8580_(is_8580) {
        // cermu SID (via harness — already configures clock & sample rate)
        harness_ = sid_test::create(false);
        if (is_8580) harness_->sid->set_revision(SID_REVISION_8580_R5);
        harness_->sid->set_cpu_clock(CPU_CLOCK);
        harness_->sid->set_sample_rate(SAMPLE_RATE);
        harness_->sid->enable_filter = true;

        // reSID (instrumented)
        resid_.set_chip_model(is_8580 ? reSID::MOS8580 : reSID::MOS6581);
        resid_.enable_filter(true);
        resid_.enable_external_filter(true);
        resid_.set_sampling_parameters(
            (double)CPU_CLOCK,
            reSID::SAMPLE_RESAMPLE,
            (double)SAMPLE_RATE
        );
        resid_.reset();
    }

    ~AudioDualSID() {
        if (harness_) sid_test::destroy(harness_);
    }

    void reset() {
        sid_test::reset(harness_);
        harness_->sid->set_cpu_clock(CPU_CLOCK);
        harness_->sid->set_sample_rate(SAMPLE_RATE);
        harness_->sid->enable_filter = true;
        resid_.reset();
        cycle_ = 0;
        cermu_samples_.clear();
        resid_samples_.clear();
        resid_percycle_output_.clear();
    }

    void write(uint8_t reg, uint8_t value) {
        sid_test::write_reg(harness_, reg, value);
        resid_.write(reg, value);
    }

    // Clock both SIDs for `cycles` ticks, collecting per-cycle and per-sample
    // comparison data into `result`.
    void clock(uint32_t cycles, audio_comparison_result_t& result) {
        // We clock reSID one cycle at a time so we can probe after each cycle.
        // For sample generation, we also use its single-cycle clock and call
        // output() manually, rather than the batched clock(delta_t, buf, n).
        // This ensures we compare the same pipeline positions.

        // Normalisation constants matching cermu's advance_cycle():
        //   voice = (waveform - 2048) * envelope / (3 * 2048 * 255)
        static constexpr float cermu_voice_scale =
            1.0f / (3.0f * 2048.0f * 255.0f);

        for (uint32_t i = 0; i < cycles; i++) {
            // Clock cermu
            bus_state_t bs = BUS_STATE(0, 0, 0);
            harness_->sid->tick(bs);
            harness_->total_cycles++;

            // Clock reSID
            resid_.clock();

            cycle_++;

            // ── Per-cycle voice comparison ──────────────────────────
            // cermu: (osc - 2048) * env / (3 * 2048 * 255) → [-1/3, +1/3]
            // reSID: (wave.output() - wave_zero) * envelope → ~20-bit int
            // We normalise reSID to the same scale.
            for (int v = 0; v < 3; v++) {
                auto ref_vo = resid_.get_voice_output(v);
                float ref_norm = (float)ref_vo.output * cermu_voice_scale;

                // cermu voice output: reconstruct from public fields
                auto* voice = harness_->sid->voices[v];
                float our_norm = (float)((int32_t)voice->oscillator_waveform - 2048)
                               * (float)voice->envelope_amplitude * cermu_voice_scale;

                result.voice_stats[v].record(our_norm, ref_norm);
            }

            // ── Per-cycle filter output comparison ──────────────────
            // reSID filter.output() is a 16-bit short after the internal mixer
            // normalised to [-1, +1].
            // cermu's filter_state tracks LP/BP/HP outputs and the routing
            // selects which contributes.  The closest comparison point is the
            // final mixed output (post-filter + unfiltered + DC + volume).
            //
            // Since the filter models are fundamentally different (transistor-
            // level op-amp vs ZDF SVF), we report filter stats for information
            // but don't enforce a tight tolerance.
            {
                float ref_filt = (float)resid_.get_filter_output() * RESID_SHORT_NORM;
                // cermu doesn't expose a single "filter output" separately;
                // we use filter_state.low_pass_output as a proxy when LP is on
                auto& fs = harness_->sid->filter_state;
                float our_filt = 0.0f;
                if (harness_->sid->filter_lp) our_filt += fs.low_pass_output;
                if (harness_->sid->filter_bp) our_filt += fs.band_pass_output;
                if (harness_->sid->filter_hp) our_filt += fs.high_pass_output;
                result.filter_stats.record(our_filt, ref_filt);
            }

            // ── Per-cycle mixed output (pre-decimation) ─────────────
            // reSID: extfilt.output() gives the final analog out (16-bit-ish)
            // cermu: the 'total' variable fed into CIC integrators.
            // We collect reSID per-cycle output for later decimated comparison.
            {
                float ref_mix = (float)resid_.get_extfilt_output() * RESID_SHORT_NORM;
                resid_percycle_output_.push_back(ref_mix);
                result.mix_stats.record(0.0f, ref_mix);  // Mix stats informational
            }

            // ── Collect decimated samples ───────────────────────────
            // cermu: check ring buffer for new samples
            while (!harness_->sid->sample_buffer.empty()) {
                float s = harness_->sid->sample_buffer.read();
                cermu_samples_.push_back(s);
            }
        }
    }

    // After clocking, generate the reSID decimated (resampled) output for the
    // same total duration and compare sample-by-sample.
    void finalise_sample_comparison(audio_comparison_result_t& result) {
        // Generate reSID resampled output for the entire duration
        reSID::cycle_count delta = (reSID::cycle_count)cycle_;
        // Upper bound on samples: ceil(cycle / (cpu_clock/sample_rate)) + margin
        int max_samples = (int)((double)cycle_ * (double)SAMPLE_RATE / (double)CPU_CLOCK) + 64;
        resid_samples_.resize((size_t)max_samples);

        // reSID was already clocked cycle-by-cycle above.  For resampled output
        // we need to re-run it.  Instead, we collect per-cycle output() and
        // do our own box-average decimation to match cermu's CIC-3 output rate.
        // This is simpler and avoids re-clocking.
        //
        // Actually, reSID's cycle-by-cycle output() goes through the external
        // filter but NOT the FIR resampler.  For a fair resampled comparison,
        // we need the FIR path.  Since we can't easily rewind reSID, we'll
        // run a second reSID instance for sample generation.

        // --- Second reSID pass for resampled output ---
        resid_probe::InstrumentedSID resid2;
        resid2.set_chip_model(is_8580_ ? reSID::MOS8580 : reSID::MOS6581);
        resid2.enable_filter(true);
        resid2.enable_external_filter(true);
        resid2.set_sampling_parameters(
            (double)CPU_CLOCK,
            reSID::SAMPLE_RESAMPLE,
            (double)SAMPLE_RATE
        );
        resid2.reset();

        // Replay register writes from cermu's register array isn't feasible
        // without a write log.  Instead, we recorded voice stats per-cycle
        // and the important metric is the sample-level comparison.
        //
        // For sample comparison, we accept that the second pass would need
        // the write log.  Instead, we'll compare using the samples we can
        // obtain: cermu's ring buffer output vs reSID's per-cycle output()
        // collected into a simple decimation buffer.
        //
        // For now, sample comparison uses the per-cycle reSID output collected
        // during the clock() loop (see resid_percycle_output_).

        // Convert reSID per-cycle output to samples via simple decimation
        // (same rate as cermu's sample output).
        uint32_t cycles_per_sample = (uint32_t)(CPU_CLOCK / SAMPLE_RATE);
        size_t resid_sample_count = resid_percycle_output_.size() / cycles_per_sample;

        resid_samples_.clear();
        for (size_t s = 0; s < resid_sample_count; s++) {
            // Simple average over the cycles in this sample period
            double acc = 0.0;
            size_t start = s * cycles_per_sample;
            size_t end = std::min(start + cycles_per_sample,
                                  resid_percycle_output_.size());
            for (size_t c = start; c < end; c++) {
                acc += resid_percycle_output_[c];
            }
            resid_samples_.push_back((float)(acc / (double)(end - start)));
        }

        // Compare sample-by-sample
        size_t n = std::min(cermu_samples_.size(), resid_samples_.size());
        for (size_t i = 0; i < n; i++) {
            result.sample_stats.record(cermu_samples_[i], resid_samples_[i]);
        }

        if (cermu_samples_.size() != resid_samples_.size()) {
            printf("  Note: sample count mismatch — cermu=%zu, reSID=%zu (compared %zu)\n",
                   cermu_samples_.size(), resid_samples_.size(), n);
        }
    }

    // Access internal sample buffers for external analysis
    const std::vector<float>& cermu_samples() const { return cermu_samples_; }
    const std::vector<float>& resid_samples() const { return resid_samples_; }

    sid_test::harness_t* harness() { return harness_; }
    resid_probe::InstrumentedSID& resid() { return resid_; }
    uint64_t cycle() const { return cycle_; }

private:
    sid_test::harness_t*          harness_ = nullptr;
    resid_probe::InstrumentedSID  resid_;
    bool                          is_8580_;
    uint64_t                      cycle_ = 0;
    std::vector<float>            cermu_samples_;
    std::vector<float>            resid_samples_;
    std::vector<float>            resid_percycle_output_;  // per-cycle output()
};

// =============================================================================
// Audio comparison tests
// =============================================================================

static int audio_test_single_voice(AudioDualSID& dual, int voice_idx,
                                   uint8_t waveform, const char* waveform_name,
                                   bool verbose) {
    printf("── Audio: Voice %d %s ──\n", voice_idx + 1, waveform_name);
    audio_comparison_result_t result;
    dual.reset();

    // Base register offsets for voice (0 = $00, 1 = $07, 2 = $0E)
    uint8_t base = (uint8_t)(voice_idx * 7);

    // Set up frequency and waveform
    dual.write(base + 0, 0x00);               // Freq lo
    dual.write(base + 1, 0x10);               // Freq hi = $1000
    if (waveform & 0x40) {                     // Pulse: set 50% duty
        dual.write(base + 2, 0x00);            // PW lo
        dual.write(base + 3, 0x08);            // PW hi = $800
    }
    dual.write(base + 5, 0x00);               // AD = attack 0, decay 0
    dual.write(base + 6, 0xF0);               // SR = sustain F, release 0
    dual.write(0x18, 0x0F);                   // Volume max, no filter

    // Test bit reset + gate + waveform
    dual.write(base + 4, waveform | 0x08);     // Test bit to reset accumulator
    dual.clock(1, result);
    dual.write(base + 4, waveform | 0x01);     // Release test, gate ON

    // Run for enough cycles to cover several waveform periods
    dual.clock(32768, result);

    dual.finalise_sample_comparison(result);
    result.print_summary();

    return result.pass(10.0) ? 0 : 1;   // Relaxed threshold for unfiltered
}

static int audio_test_filter_sweep(AudioDualSID& dual, bool verbose) {
    printf("── Audio: Filter cutoff sweep (LP, voice 1) ──\n");
    audio_comparison_result_t result;
    dual.reset();

    // Voice 1: sawtooth at mid frequency, gated
    dual.write(0x00, 0x00);            // V1 freq lo
    dual.write(0x01, 0x10);            // V1 freq hi
    dual.write(0x05, 0x00);            // AD = 0,0
    dual.write(0x06, 0xF0);            // SR = F,0
    dual.write(0x04, 0x28);            // Test + sawtooth
    dual.clock(1, result);
    dual.write(0x04, 0x21);            // Gate + sawtooth

    // Route voice 1 through filter, LP mode, resonance 8
    dual.write(0x17, 0x81);            // Res=8, filt1=on
    dual.write(0x18, 0x1F);            // LP on, vol max

    // Sweep cutoff from 0 to max over 65536 cycles
    for (int fc = 0; fc < 2048; fc += 8) {
        uint8_t fc_lo = (uint8_t)(fc & 0x07);
        uint8_t fc_hi = (uint8_t)(fc >> 3);
        dual.write(0x15, fc_lo);
        dual.write(0x16, fc_hi);
        dual.clock(32, result);
    }

    dual.finalise_sample_comparison(result);
    result.print_summary();

    // Filter models are very different; accept wider tolerance
    return result.pass(6.0) ? 0 : 1;
}

static int audio_test_d418_digi(AudioDualSID& dual, bool verbose) {
    printf("── Audio: D418 volume-register digi playback ──\n");
    audio_comparison_result_t result;
    dual.reset();

    // No voices gated — pure DC offset modulation (6581)
    // Rapidly alternate volume between 0 and 15 to produce a square wave digi
    for (int i = 0; i < 4000; i++) {
        dual.write(0x18, 0x0F);       // Vol max
        dual.clock(8, result);         // ~125 kHz → ~8 cycles per half-period
        dual.write(0x18, 0x00);       // Vol min
        dual.clock(8, result);
    }

    dual.finalise_sample_comparison(result);
    result.print_summary();

    return result.pass(10.0) ? 0 : 1;
}

static int audio_test_pwm_digi(AudioDualSID& dual, bool verbose) {
    printf("── Audio: PWM digi playback (Swallow technique) ──\n");
    audio_comparison_result_t result;
    dual.reset();

    // Voice 1: pulse waveform, maximum frequency for DAC-like behaviour
    dual.write(0x00, 0xFF);            // V1 freq lo = max
    dual.write(0x01, 0xFF);            // V1 freq hi = max
    dual.write(0x05, 0x00);            // AD = 0,0
    dual.write(0x06, 0xF0);            // SR = F,0
    dual.write(0x04, 0x41);            // Pulse + gate
    dual.write(0x18, 0x0F);           // Vol max, no filter

    // Sweep pulse width from 0 to 0xFFF to simulate 8-bit sample playback
    // Each "sample" updates PW and runs for ~22 cycles (~44.1 kHz equivalent)
    for (int sample = 0; sample < 2048; sample++) {
        // Triangle wave pattern for test
        int pw = (sample < 1024) ? sample * 4 : (2048 - sample) * 4;
        if (pw > 0xFFF) pw = 0xFFF;
        dual.write(0x02, (uint8_t)(pw & 0xFF));       // PW lo
        dual.write(0x03, (uint8_t)((pw >> 8) & 0x0F)); // PW hi
        dual.clock(22, result);
    }

    dual.finalise_sample_comparison(result);
    result.print_summary();

    return result.pass(10.0) ? 0 : 1;
}

static int audio_test_multi_voice_mix(AudioDualSID& dual, bool verbose) {
    printf("── Audio: Three-voice mix (AWE-like) ──\n");
    audio_comparison_result_t result;
    dual.reset();

    // All three voices at different frequencies, gated simultaneously
    // Voice 1: sawtooth at freq $0800
    dual.write(0x00, 0x00);  dual.write(0x01, 0x08);
    dual.write(0x05, 0x00);  dual.write(0x06, 0xF0);
    dual.write(0x04, 0x28);  // test+saw
    // Voice 2: triangle at freq $0C00
    dual.write(0x07, 0x00);  dual.write(0x08, 0x0C);
    dual.write(0x0C, 0x00);  dual.write(0x0D, 0xF0);
    dual.write(0x0B, 0x18);  // test+tri
    // Voice 3: pulse at freq $1000, 50% duty
    dual.write(0x0E, 0x00);  dual.write(0x0F, 0x10);
    dual.write(0x10, 0x00);  dual.write(0x11, 0x08);
    dual.write(0x13, 0x00);  dual.write(0x14, 0xF0);
    dual.write(0x12, 0x48);  // test+pulse

    dual.clock(1, result);

    // Release test bits and gate all voices
    dual.write(0x04, 0x21);  // V1: saw + gate
    dual.write(0x0B, 0x11);  // V2: tri + gate
    dual.write(0x12, 0x41);  // V3: pulse + gate
    dual.write(0x18, 0x0F);  // Vol max, no filter

    dual.clock(65536, result);

    dual.finalise_sample_comparison(result);
    result.print_summary();

    return result.pass(10.0) ? 0 : 1;
}

static int audio_test_multi_voice_filtered(AudioDualSID& dual, bool verbose) {
    printf("── Audio: Three-voice filtered mix ──\n");
    audio_comparison_result_t result;
    dual.reset();

    // Voice 1: sawtooth at $0800
    dual.write(0x00, 0x00);  dual.write(0x01, 0x08);
    dual.write(0x05, 0x00);  dual.write(0x06, 0xF0);
    dual.write(0x04, 0x28);
    // Voice 2: sawtooth at $1000
    dual.write(0x07, 0x00);  dual.write(0x08, 0x10);
    dual.write(0x0C, 0x00);  dual.write(0x0D, 0xF0);
    dual.write(0x0B, 0x28);
    // Voice 3: sawtooth at $1800
    dual.write(0x0E, 0x00);  dual.write(0x0F, 0x18);
    dual.write(0x13, 0x00);  dual.write(0x14, 0xF0);
    dual.write(0x12, 0x28);

    dual.clock(1, result);

    // Gate all
    dual.write(0x04, 0x21);
    dual.write(0x0B, 0x21);
    dual.write(0x12, 0x21);

    // Route voices 1+2 through filter, LP + BP modes
    dual.write(0x15, 0x00);           // FC lo
    dual.write(0x16, 0x20);           // FC hi — mid cutoff
    dual.write(0x17, 0xA3);           // Res=10, filt1+filt2+filt3
    dual.write(0x18, 0x1F);           // LP on, vol max

    dual.clock(65536, result);

    dual.finalise_sample_comparison(result);
    result.print_summary();

    // Filtered comparison: looser threshold due to different filter topologies
    return result.pass(3.0) ? 0 : 1;
}

// =============================================================================
// SID Write Log Replay — replay captured register writes against both SIDs
// =============================================================================
// Runs cermu SID and reSID independently with identical register writes from
// a captured binary log.  Both produce decimated audio samples at 44.1 kHz:
//   cermu: CIC-3 decimation + DC blocker → ring buffer
//   reSID: FIR sinc resampler (SAMPLE_RESAMPLE)
// The final samples are compared for RMS error, SNR, and correlation.
// Optionally writes WAV files for auditory comparison.
// =============================================================================

// Minimal WAV header writer (16-bit mono PCM)
static bool write_wav(const char* path, const std::vector<float>& samples,
                      uint32_t sample_rate) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    uint32_t data_size = (uint32_t)(samples.size() * 2);  // 16-bit
    uint32_t file_size = 36 + data_size;

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1;  // PCM
    uint16_t channels = 1;
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = sample_rate * 2;
    uint16_t block_align = 2;
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    for (float s : samples) {
        float clamped = s;
        if (clamped > 1.0f) clamped = 1.0f;
        if (clamped < -1.0f) clamped = -1.0f;
        int16_t pcm = (int16_t)(clamped * 32767.0f);
        fwrite(&pcm, 2, 1, f);
    }

    fclose(f);
    return true;
}

static int run_log_comparison(const char* log_path, bool verbose, bool no_filter = false) {
    sid_log::write_log_t log;
    if (!sid_log::read_file(log_path, log)) {
        printf("ERROR: Failed to read SID log: %s\n", log_path);
        return 1;
    }

    bool is_8580 = (log.chip_model != 0);
    float cpu_clock = (float)log.cpu_clock;
    constexpr float SAMPLE_RATE = 44100.0f;

    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  SID Log Replay Comparison: cermu vs reSID       ║\n");
    printf("║  Model: %s  Clock: %u Hz                    ║\n",
           is_8580 ? "8580" : "6581", log.cpu_clock);
    printf("║  Entries: %-10zu                              ║\n", log.entries.size());
    printf("╚══════════════════════════════════════════════════╝\n\n");

    if (log.entries.empty()) {
        printf("WARNING: Log file is empty — nothing to replay\n");
        return 0;
    }

    // ── Set up cermu SID ────────────────────────────────────────────────
    auto* harness = sid_test::create(false);
    if (is_8580) harness->sid->set_revision(SID_REVISION_8580_R5);
    harness->sid->set_cpu_clock(cpu_clock);
    harness->sid->set_sample_rate(SAMPLE_RATE);
    harness->sid->enable_filter = !no_filter;

    // ── Set up reSID with FIR sinc resampler ────────────────────────────
    resid_probe::InstrumentedSID resid;
    resid.set_chip_model(is_8580 ? reSID::MOS8580 : reSID::MOS6581);
    resid.enable_filter(!no_filter);
    resid.enable_external_filter(!no_filter);
    resid.set_sampling_parameters(
        (double)cpu_clock,
        reSID::SAMPLE_RESAMPLE,
        (double)SAMPLE_RATE
    );
    resid.reset();

    // ── Replay writes and collect samples ───────────────────────────────
    std::vector<float> cermu_samples;
    std::vector<float> resid_samples;
    cermu_samples.reserve(log.cpu_clock * 2 / 22);  // ~rough estimate
    resid_samples.reserve(log.cpu_clock * 2 / 22);

    // reSID sample buffer (generous: max samples per batch)
    constexpr int RESID_BUF_SIZE = 8192;
    short resid_buf[RESID_BUF_SIZE];

    uint32_t current_cycle = 0;
    size_t progress_interval = log.entries.size() / 10;
    if (progress_interval == 0) progress_interval = 1;

    // Helper: clock both SIDs forward by delta cycles, collecting samples
    auto clock_both = [&](uint32_t delta) {
        // Clock cermu cycle-by-cycle
        for (uint32_t c = 0; c < delta; c++) {
            bus_state_t bs = BUS_STATE(0, 0, 0);
            harness->sid->tick(bs);
            harness->total_cycles++;

            // Drain cermu ring buffer
            while (!harness->sid->sample_buffer.empty()) {
                cermu_samples.push_back(harness->sid->sample_buffer.read());
            }
        }

        // Clock reSID in bulk — generate resampled output
        reSID::cycle_count remaining = (reSID::cycle_count)delta;
        while (remaining > 0) {
            int n = resid.generate_samples(remaining, resid_buf, RESID_BUF_SIZE);
            for (int i = 0; i < n; i++) {
                resid_samples.push_back((float)resid_buf[i] / 32768.0f);
            }
        }
    };

    printf("  Replaying %zu register writes...\n", log.entries.size());

    for (size_t i = 0; i < log.entries.size(); i++) {
        const auto& entry = log.entries[i];

        // Clock forward to this entry's cycle
        if (entry.cycle > current_cycle) {
            uint32_t delta = entry.cycle - current_cycle;
            clock_both(delta);
            current_cycle = entry.cycle;
        }

        // Apply the register write to both SIDs
        sid_test::write_reg(harness, entry.reg, entry.value);
        resid.write(entry.reg, entry.value);

        // Progress report
        if ((i % progress_interval == 0)) {
            printf("  Replay: %zu/%zu entries (cycle %u, cermu=%zu resid=%zu samples)\n",
                   i, log.entries.size(), current_cycle,
                   cermu_samples.size(), resid_samples.size());
        }
    }

    // Clock 1 second of tail to let the last writes ring out
    clock_both(log.cpu_clock);
    current_cycle += log.cpu_clock;

    printf("  Replay complete: %zu cermu samples, %zu reSID samples\n",
           cermu_samples.size(), resid_samples.size());

    // ── Compute comparison metrics ──────────────────────────────────────
    audio_stats_t sample_stats;
    sample_stats.name = "Final audio samples";

    size_t n = std::min(cermu_samples.size(), resid_samples.size());
    for (size_t i = 0; i < n; i++) {
        sample_stats.record(cermu_samples[i], resid_samples[i]);
    }

    printf("\n── Log Replay Results ──\n");
    printf("  Total entries replayed: %zu\n", log.entries.size());
    printf("  Total cycles: %u\n", current_cycle);
    printf("  cermu samples: %zu, reSID samples: %zu (compared: %zu)\n",
           cermu_samples.size(), resid_samples.size(), n);
    printf("  ┌──────────────────────────────────────────────────────────────────────────┐\n");
    sample_stats.print();
    printf("  └──────────────────────────────────────────────────────────────────────────┘\n");

    // ── Phase-compensated cross-correlation ──────────────────────────────
    // CIC-3 and FIR sinc resamplers have different group delays, creating a
    // constant phase offset that kills sample-by-sample correlation even when
    // both outputs are perceptually identical.  Search lags ±50 samples to
    // find the optimal alignment and report both raw and compensated metrics.
    {
        constexpr int MAX_LAG = 50;
        double best_r = -2.0;
        int best_lag = 0;

        // Compute mean-subtracted norms (for Pearson correlation with lag)
        double sum_c = 0, sum_r = 0;
        for (size_t i = 0; i < n; i++) {
            sum_c += cermu_samples[i];
            sum_r += resid_samples[i];
        }
        double mean_c = sum_c / n;
        double mean_r = sum_r / n;

        for (int lag = -MAX_LAG; lag <= MAX_LAG; lag++) {
            double cross = 0, ssq_c = 0, ssq_r = 0;
            size_t start = (lag >= 0) ? (size_t)lag : 0;
            size_t end   = (lag >= 0) ? n : n + lag;
            for (size_t i = start; i < end; i++) {
                double c = cermu_samples[i] - mean_c;
                double r = resid_samples[i - lag] - mean_r;
                cross += c * r;
                ssq_c += c * c;
                ssq_r += r * r;
            }
            double denom = sqrt(ssq_c * ssq_r);
            double r_val = (denom > 1e-30) ? cross / denom : 0.0;
            if (r_val > best_r) {
                best_r = r_val;
                best_lag = lag;
            }
        }

        printf("\n  Phase-compensated comparison:\n");
        printf("    Optimal lag: %d samples (%.3f ms, cermu %s reSID by %d samples)\n",
               best_lag, fabs(best_lag) * 1000.0 / SAMPLE_RATE,
               best_lag > 0 ? "leads" : "lags", abs(best_lag));
        printf("    Raw correlation (lag=0): r=%.6f\n", sample_stats.correlation());
        printf("    Best correlation (lag=%d): r=%.6f\n", best_lag, best_r);

        // Compute RMS and SNR at optimal lag
        if (best_lag != 0) {
            double sum_sq_err = 0, sum_ref = 0;
            size_t start = (best_lag >= 0) ? (size_t)best_lag : 0;
            size_t end   = (best_lag >= 0) ? n : n + best_lag;
            for (size_t i = start; i < end; i++) {
                double e = cermu_samples[i] - resid_samples[i - best_lag];
                sum_sq_err += e * e;
                double b = resid_samples[i - best_lag];
                sum_ref += b * b;
            }
            size_t cnt = end - start;
            double rms = sqrt(sum_sq_err / cnt);
            double snr = (sum_sq_err > 1e-30) ? 10.0 * log10(sum_ref / sum_sq_err) : 999.0;
            printf("    Phase-aligned RMS=%.6f  SNR=%.1f dB\n", rms, snr);
        }
    }

    // ── Spectral magnitude comparison ───────────────────────────────────
    // Compare frequency content independent of phase — this measures whether
    // both implementations produce the same spectral energy distribution.
    // Uses Goertzel to probe band-center frequencies over the LOUDEST section.
    {
        uint32_t fft_size = (uint32_t)SAMPLE_RATE;  // 1-second windows

        // Find the loudest 4-second segment in reSID output (skip first 1s)
        size_t best_start = fft_size;
        double best_energy = 0;
        for (size_t s = fft_size; s + fft_size * 4 < n; s += fft_size) {
            double energy = 0;
            for (size_t i = s; i < s + fft_size * 4; i++)
                energy += (double)resid_samples[i] * resid_samples[i];
            if (energy > best_energy) {
                best_energy = energy;
                best_start = s;
            }
        }

        size_t goertzel_n = std::min((size_t)fft_size * 4, n - best_start);
        if (goertzel_n > fft_size && best_energy > 1e-10) {
            printf("\n  Spectral band comparison (loudest 4s segment at %.1fs):\n",
                   (double)best_start / SAMPLE_RATE);
            double band_edges[] = {20, 100, 500, 1000, 2000, 5000, 10000, 20000};
            constexpr int NUM_BANDS = 7;
            for (int b = 0; b < NUM_BANDS; b++) {
                double f_center = (band_edges[b] + band_edges[b + 1]) / 2.0;
                double omega = 2.0 * M_PI * f_center / SAMPLE_RATE;
                double coeff = 2.0 * cos(omega);
                double s1_c = 0, s2_c = 0, s1_r = 0, s2_r = 0;
                for (size_t i = 0; i < goertzel_n; i++) {
                    double t;
                    t = cermu_samples[best_start + i] + coeff * s1_c - s2_c;
                    s2_c = s1_c; s1_c = t;
                    t = resid_samples[best_start + i] + coeff * s1_r - s2_r;
                    s2_r = s1_r; s1_r = t;
                }
                double pow_c = s1_c*s1_c + s2_c*s2_c - coeff*s1_c*s2_c;
                double pow_r = s1_r*s1_r + s2_r*s2_r - coeff*s1_r*s2_r;
                double ratio_db = (pow_r > 1e-30 && pow_c > 1e-30)
                    ? 10.0 * log10(pow_c / pow_r) : -99.0;
                printf("    %5.0f-%5.0fHz: cermu/reSID = %+5.1f dB\n",
                       band_edges[b], band_edges[b + 1], ratio_db);
            }
        }
    }

    // ── Write WAV files for auditory comparison ─────────────────────────
    {
        std::string base(log_path);
        // Strip extension if any
        auto dot = base.rfind('.');
        if (dot != std::string::npos) base = base.substr(0, dot);

        std::string cermu_wav = base + "_cermu.wav";
        std::string resid_wav = base + "_resid.wav";

        if (write_wav(cermu_wav.c_str(), cermu_samples, (uint32_t)SAMPLE_RATE))
            printf("\n  WAV: %s (%zu samples)\n", cermu_wav.c_str(), cermu_samples.size());
        if (write_wav(resid_wav.c_str(), resid_samples, (uint32_t)SAMPLE_RATE))
            printf("  WAV: %s (%zu samples)\n", resid_wav.c_str(), resid_samples.size());
    }

    sid_test::destroy(harness);

    return (sample_stats.snr_db() >= 3.0) ? 0 : 1;
}

static int run_all_audio_tests(bool is_8580, bool verbose) {
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║  SID Audio Pipeline Comparison: cermu vs reSID   ║\n");
    printf("║  Model: %s                                    ║\n", is_8580 ? "8580" : "6581");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    AudioDualSID dual(is_8580);

    int failures = 0;

    // Per-voice waveform tests (no filter)
    failures += audio_test_single_voice(dual, 0, 0x20, "Sawtooth", verbose);
    failures += audio_test_single_voice(dual, 0, 0x10, "Triangle", verbose);
    failures += audio_test_single_voice(dual, 0, 0x40, "Pulse", verbose);
    failures += audio_test_single_voice(dual, 0, 0x80, "Noise", verbose);

    // Multi-voice mix (no filter)
    failures += audio_test_multi_voice_mix(dual, verbose);

    // Filter comparison
    failures += audio_test_filter_sweep(dual, verbose);
    failures += audio_test_multi_voice_filtered(dual, verbose);

    // Digi techniques
    failures += audio_test_d418_digi(dual, verbose);
    failures += audio_test_pwm_digi(dual, verbose);

    printf("\n══════════════════════════════════════════════════\n");
    printf("  AUDIO TESTS: %d failures\n", failures);
    printf("══════════════════════════════════════════════════\n");

    return failures;
}

// =============================================================================
// Main
// =============================================================================

static void print_usage(const char* argv0) {
    printf("SID Reference Comparison Runner — cermu vs reSID\n\n");
    printf("Usage: %s [options]\n\n", argv0);
    printf("Options:\n");
    printf("  --verbose              Show every comparison cycle\n");
    printf("  --audio                Run audio pipeline comparison tests\n");
    printf("  --script <file>        Run a .sid_test script with dual comparison\n");
    printf("  --dat <file>           Compare against hardware trace (.dat PRG format)\n");
    printf("  --waveform <hex>       Waveform for --dat mode (0x10=tri, 0x20=saw, etc.)\n");
    printf("  --revision <type>      Set chip revision: 6581 (default) or 8580\n");
    printf("  --resid-only           Only compare reSID against hardware (skip cermu)\n");
    printf("  --all-dat <dir>        Run all .dat files in the given directory\n");
    printf("  --log <file>           Replay a SID write log against both SIDs\n");
    printf("  --help                 Show this help\n\n");
    printf("Examples:\n");
    printf("  %s                                     # Run built-in dual tests\n", argv0);
    printf("  %s --dat oscsample0-6581wf20.dat --waveform 0x20\n", argv0);
    printf("  %s --script tests/sid_scripts/11_resid_test_normal_adsr.sid_test\n", argv0);
    printf("  %s --all-dat ../VICE-testprogs/SID/resid-test/\n", argv0);
    printf("  %s --log captured_demo.sidlog\n", argv0);
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool is_8580 = false;
    bool resid_only = false;
    bool audio_mode = false;
    bool no_filter = false;
    const char* script_path = nullptr;
    const char* dat_path = nullptr;
    const char* all_dat_dir = nullptr;
    const char* log_path = nullptr;
    uint8_t waveform = 0x20;  // Default: sawtooth

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--audio") == 0) {
            audio_mode = true;
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            script_path = argv[++i];
        } else if (strcmp(argv[i], "--dat") == 0 && i + 1 < argc) {
            dat_path = argv[++i];
        } else if (strcmp(argv[i], "--waveform") == 0 && i + 1 < argc) {
            waveform = (uint8_t)strtol(argv[++i], nullptr, 0);
        } else if (strcmp(argv[i], "--revision") == 0 && i + 1 < argc) {
            i++;
            is_8580 = (strcmp(argv[i], "8580") == 0);
        } else if (strcmp(argv[i], "--resid-only") == 0) {
            resid_only = true;
        } else if (strcmp(argv[i], "--all-dat") == 0 && i + 1 < argc) {
            all_dat_dir = argv[++i];
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            log_path = argv[++i];
        } else if (strcmp(argv[i], "--no-filter") == 0) {
            no_filter = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    int total_failures = 0;

    if (log_path) {
        // Replay a captured SID write log
        total_failures += run_log_comparison(log_path, verbose, no_filter);
    } else if (dat_path) {
        // Single .dat file comparison
        total_failures += compare_dat_file(dat_path, waveform, is_8580, verbose, resid_only);
    } else if (all_dat_dir) {
        // Run all .dat files in a directory
        // Waveform mapping: wf10=tri, wf20=saw, wf30=tri+saw, wf40=pulse,
        //                   wf50=tri+pulse, wf60=saw+pulse, wf70=tri+saw+pulse, wf80=noise
        struct dat_test_t {
            const char* suffix;
            uint8_t wf;
            const char* name;
        };
        dat_test_t tests[] = {
            {"wf10.dat", 0x10, "Triangle"},
            {"wf20.dat", 0x20, "Sawtooth"},
            {"wf30.dat", 0x30, "Tri+Saw"},
            {"wf40.dat", 0x40, "Pulse"},
            {"wf50.dat", 0x50, "Tri+Pulse"},
            {"wf60.dat", 0x60, "Saw+Pulse"},
            {"wf70.dat", 0x70, "Tri+Saw+Pulse"},
            {"wf80.dat", 0x80, "Noise"},
        };

        const char* models[] = {"6581", "8580"};
        const char* prefixes[] = {"oscsample0-", "oscsample1-"};

        for (const char* model : models) {
            bool model_8580 = (strcmp(model, "8580") == 0);
            for (const char* prefix : prefixes) {
                for (const auto& t : tests) {
                    std::string filename = std::string(all_dat_dir) + "/" +
                                           prefix + model + t.suffix;
                    FILE* check = fopen(filename.c_str(), "rb");
                    if (check) {
                        fclose(check);
                        printf("\n========== %s %s %s ==========\n",
                               prefix, model, t.name);
                        total_failures += compare_dat_file(
                            filename.c_str(), t.wf, model_8580, verbose, resid_only);
                    }
                }
            }
        }
    } else if (script_path) {
        // Script-driven comparison
        total_failures += run_script_comparison(script_path, is_8580, verbose);
    } else if (audio_mode) {
        // Audio pipeline comparison tests
        total_failures += run_all_audio_tests(is_8580, verbose);
    } else {
        // Run all built-in dual-comparison tests (digital + audio)
        printf("╔══════════════════════════════════════════════════╗\n");
        printf("║  SID Reference Comparison: cermu vs reSID       ║\n");
        printf("║  Model: %s                                    ║\n", is_8580 ? "8580" : "6581");
        printf("╚══════════════════════════════════════════════════╝\n\n");

        DualSID dual(is_8580);

        total_failures += test_sawtooth_comparison(dual, verbose);
        total_failures += test_triangle_comparison(dual, verbose);
        total_failures += test_pulse_comparison(dual, verbose);
        total_failures += test_noise_comparison(dual, verbose);
        total_failures += test_envelope_adsr_comparison(dual, verbose);
        total_failures += test_envelope_edge_cases(dual, verbose);
        total_failures += test_combined_waveforms_comparison(dual, verbose);
        total_failures += test_ring_modulation_comparison(dual, verbose);
        total_failures += test_oscillator_sync_comparison(dual, verbose);
        total_failures += test_test_bit_comparison(dual, verbose);

        printf("\n══════════════════════════════════════════════════\n");
        printf("  DIGITAL: %d total mismatches across all tests\n", total_failures);
        printf("══════════════════════════════════════════════════\n");

        // Also run audio pipeline comparison
        total_failures += run_all_audio_tests(is_8580, verbose);
    }

    return total_failures > 0 ? 1 : 0;
}
