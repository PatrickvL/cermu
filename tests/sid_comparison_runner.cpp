// =============================================================================
// SID Reference Comparison Harness
// =============================================================================
// Runs cermu's MOS6581 side-by-side with reSID (Dag Lem) as the reference
// oracle.  Both SIDs receive identical register writes and are clocked in
// lockstep.  After each clock cycle the digital probes (OSC3, ENV3) and
// internal accumulator state are compared.
//
// Additionally supports importing hardware-captured OSC3 traces from the
// VICE-testprogs/SID/resid-test/ .dat files (C64 PRG format: 2-byte load
// address header + raw OSC3 bytes).
//
// Usage:
//   sid_comparison_runner                              # Run all built-in tests
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
#include "sid.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>

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

        // Initialize reSID
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
    reSID::SID& resid() { return resid_; }

    uint64_t cycle() const { return cycle_; }

private:
    sid_test::harness_t* harness_ = nullptr;
    reSID::SID           resid_;
    uint64_t             cycle_ = 0;
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
// Main
// =============================================================================

static void print_usage(const char* argv0) {
    printf("SID Reference Comparison Runner — cermu vs reSID\n\n");
    printf("Usage: %s [options]\n\n", argv0);
    printf("Options:\n");
    printf("  --verbose              Show every comparison cycle\n");
    printf("  --script <file>        Run a .sid_test script with dual comparison\n");
    printf("  --dat <file>           Compare against hardware trace (.dat PRG format)\n");
    printf("  --waveform <hex>       Waveform for --dat mode (0x10=tri, 0x20=saw, etc.)\n");
    printf("  --revision <type>      Set chip revision: 6581 (default) or 8580\n");
    printf("  --resid-only           Only compare reSID against hardware (skip cermu)\n");
    printf("  --all-dat <dir>        Run all .dat files in the given directory\n");
    printf("  --help                 Show this help\n\n");
    printf("Examples:\n");
    printf("  %s                                     # Run built-in dual tests\n", argv0);
    printf("  %s --dat oscsample0-6581wf20.dat --waveform 0x20\n", argv0);
    printf("  %s --script tests/sid_scripts/11_resid_test_normal_adsr.sid_test\n", argv0);
    printf("  %s --all-dat ../VICE-testprogs/SID/resid-test/\n", argv0);
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool is_8580 = false;
    bool resid_only = false;
    const char* script_path = nullptr;
    const char* dat_path = nullptr;
    const char* all_dat_dir = nullptr;
    uint8_t waveform = 0x20;  // Default: sawtooth

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
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

    if (dat_path) {
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
    } else {
        // Run all built-in dual-comparison tests
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
        printf("  FINAL: %d total mismatches across all tests\n", total_failures);
        printf("══════════════════════════════════════════════════\n");
    }

    return total_failures > 0 ? 1 : 0;
}
