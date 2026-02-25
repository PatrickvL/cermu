#pragma once
// =============================================================================
// NES APU Digital Test Harness
// =============================================================================
// Standalone test framework for verifying the NES APU (2A03) digital pipeline.
//
// Unlike the SID which has OSC3/ENV3 test probes, the NES APU can be verified
// by reading individual channel outputs and the mixed sample output.
//
// For each channel we test:
//   - Envelope generator (divider, decay, loop)
//   - Length counter (table, halt, enable/disable)
//   - Timer/sequencer (period, waveform output)
//   - Channel-specific features (sweep, linear counter, LFSR, DMC)
//
// Usage:
//   1. Create harness: apu_test::create()
//   2. Run built-in tests: apu_test::run_all_builtin_tests()
//   3. Or build scripts programmatically and run them
//   4. Destroy: apu_test::destroy()
// =============================================================================

#include "../chip/cpu/fam65xx/nes6502.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>

namespace apu_test {

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

// APU register addresses ($4000-$4017)
constexpr uint16_t REG_SQ1_VOL   = 0x4000;
constexpr uint16_t REG_SQ1_SWEEP = 0x4001;
constexpr uint16_t REG_SQ1_LO    = 0x4002;
constexpr uint16_t REG_SQ1_HI    = 0x4003;
constexpr uint16_t REG_SQ2_VOL   = 0x4004;
constexpr uint16_t REG_SQ2_SWEEP = 0x4005;
constexpr uint16_t REG_SQ2_LO    = 0x4006;
constexpr uint16_t REG_SQ2_HI    = 0x4007;
constexpr uint16_t REG_TRI_LIN   = 0x4008;
constexpr uint16_t REG_TRI_LO    = 0x400A;
constexpr uint16_t REG_TRI_HI    = 0x400B;
constexpr uint16_t REG_NOISE_VOL = 0x400C;
constexpr uint16_t REG_NOISE_LO  = 0x400E;
constexpr uint16_t REG_NOISE_HI  = 0x400F;
constexpr uint16_t REG_DMC_FREQ  = 0x4010;
constexpr uint16_t REG_DMC_RAW   = 0x4011;
constexpr uint16_t REG_DMC_START = 0x4012;
constexpr uint16_t REG_DMC_LEN   = 0x4013;
constexpr uint16_t REG_STATUS    = 0x4015;
constexpr uint16_t REG_FRAME_CTR = 0x4017;

// ─────────────────────────────────────────────────────────────────────────────
// Script command types
// ─────────────────────────────────────────────────────────────────────────────

enum class cmd_type_t : uint8_t {
    RESET,
    WRITE,
    RUN,                    // Run N CPU cycles
    EXPECT_PULSE1,          // Assert pulse1.output() == value
    EXPECT_PULSE2,          // Assert pulse2.output() == value
    EXPECT_TRIANGLE,        // Assert triangle.output() == value
    EXPECT_NOISE,           // Assert noise.output() == value
    EXPECT_DMC,             // Assert dmc.output() == value
    EXPECT_SAMPLE_RANGE,    // Assert sample() in [lo, hi]
    EXPECT_NOISE_LFSR,      // Assert noise shift register
    EXPECT_ENVELOPE,        // Assert envelope output for a channel
    EXPECT_LENGTH_ACTIVE,   // Assert length counter active/inactive
    LABEL,
};

struct command_t {
    cmd_type_t type;
    uint16_t   addr;        // Register address (for WRITE)
    uint8_t    value;       // Value (for WRITE, EXPECT_*)
    uint32_t   cycles;      // Cycle count (for RUN)
    uint8_t    channel;     // Channel index (0=p1, 1=p2, 2=tri, 3=noise, 4=dmc)
    float      lo, hi;      // Range (for EXPECT_SAMPLE_RANGE)
    uint16_t   lfsr;        // Expected LFSR value
    bool       bool_val;    // Expected bool (for EXPECT_LENGTH_ACTIVE)
    char       label[32];
    int        line_number;
};

// ─────────────────────────────────────────────────────────────────────────────
// Test result
// ─────────────────────────────────────────────────────────────────────────────

enum class result_type_t : uint8_t { PASS, FAIL, INFO };

struct result_entry_t {
    result_type_t type;
    uint32_t      cycle;
    const char*   check_name;
    int           line_number;
    char          message[256];
};

// ─────────────────────────────────────────────────────────────────────────────
// Test script
// ─────────────────────────────────────────────────────────────────────────────

struct test_script_t {
    std::string            name;
    std::string            description;
    std::vector<command_t> commands;
};

// ─────────────────────────────────────────────────────────────────────────────
// Test harness state
// ─────────────────────────────────────────────────────────────────────────────

struct harness_t {
    nes6502_apu::APU* apu;
    bool              apu_owned;
    uint32_t          total_cycles;
    std::vector<result_entry_t> results;
    int               pass_count;
    int               fail_count;
    bool              verbose;
};

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/// Create a new test harness with a fresh APU instance.
harness_t* create(bool verbose = false);

/// Destroy harness (and owned APU if applicable).
void destroy(harness_t* h);

/// Reset harness state and APU.
void reset(harness_t* h);

// ── Direct APU access ───────────────────────────────────────────────────────

/// Write to APU register ($4000-$4017)
void write_reg(harness_t* h, uint16_t addr, uint8_t value);

/// Read APU status ($4015)
uint8_t read_status(harness_t* h);

/// Clock N CPU cycles
void clock_cycles(harness_t* h, uint32_t n);

/// Get individual channel outputs
uint8_t get_pulse1_output(harness_t* h);
uint8_t get_pulse2_output(harness_t* h);
uint8_t get_triangle_output(harness_t* h);
uint8_t get_noise_output(harness_t* h);
uint8_t get_dmc_output(harness_t* h);

/// Get mixed sample
float get_sample(harness_t* h);

// ── Script execution ────────────────────────────────────────────────────────

/// Execute a test script. Returns number of failures.
int run_script(harness_t* h, const test_script_t* script);

/// Print results summary.
void print_results(const harness_t* h, const test_script_t* script);

// ── Built-in test suites ────────────────────────────────────────────────────

/// Run all built-in APU verification tests. Returns total failures.
int run_all_builtin_tests(harness_t* h, bool verbose = true);

/// Individual test categories (each returns failure count)
int test_length_counter_table(harness_t* h);
int test_length_counter_halt(harness_t* h);
int test_length_counter_enable(harness_t* h);
int test_envelope_constant(harness_t* h);
int test_envelope_decay(harness_t* h);
int test_envelope_loop(harness_t* h);
int test_pulse_duty_cycles(harness_t* h);
int test_pulse_sweep_basic(harness_t* h);
int test_pulse_sweep_negate(harness_t* h);
int test_pulse_sweep_muting(harness_t* h);
int test_triangle_waveform(harness_t* h);
int test_triangle_linear_counter(harness_t* h);
int test_noise_lfsr_long_mode(harness_t* h);
int test_noise_lfsr_short_mode(harness_t* h);
int test_dmc_direct_load(harness_t* h);
int test_frame_counter_4step(harness_t* h);
int test_frame_counter_5step(harness_t* h);
int test_mixer_formula(harness_t* h);
int test_status_register(harness_t* h);

} // namespace apu_test
