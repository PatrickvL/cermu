#pragma once
// =============================================================================
// SID MOS6581/8580 Digital Test Harness
// =============================================================================
// Standalone test framework for verifying the SID digital pipeline against
// reference data (perfect6581 / reSID / reSIDfp).
//
// Design inspired by libsidplayfp/resid-test scripting format.
//
// The SID exposes two "test probes" for the digital domain:
//   $D41B (OSC3) — Upper 8 bits of voice 3 waveform output
//   $D41C (ENV3) — Current 8-bit envelope value of voice 3
//
// Since all three voices use identical digital logic, proving voice 3
// correct via OSC3/ENV3 proves all voices correct.
//
// Usage:
//   1. Create a SID instance via sid_test_create()
//   2. Load a test script (text or embedded) via sid_test_load_script()
//      or build commands programmatically via sid_test_add_*()
//   3. Execute with sid_test_run()
//   4. Check results with sid_test_get_results()
//   5. Destroy with sid_test_destroy()
//
// Script format (compatible with resid-test, extended):
//   reset                        # Reset SID to initial state
//   revision, 6581|8580          # Set chip revision
//   write, <reg>, <value>        # Write to SID register (hex or decimal)
//   run, <cycles>                # Run N PHI2 cycles
//   check_osc3                   # Enable OSC3 recording during next run
//   check_env3                   # Enable ENV3 recording during next run
//   expect_osc3, <value>         # Assert current OSC3 == value
//   expect_env3, <value>         # Assert current ENV3 == value
//   expect_acc, <voice>, <value> # Assert accumulator of voice (0-2) == value
//   snapshot                     # Record OSC3+ENV3 at this cycle
//   label, <name>                # Named marker for reference data alignment
//   # comment                    # Comments (also inline after commands)
// =============================================================================

#include "../chip/sound/mos6581.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

namespace sid_test {

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

constexpr uint32_t MAX_TRACE_SAMPLES   = 1024 * 1024;  // 1M cycles max trace
constexpr uint32_t MAX_SCRIPT_COMMANDS = 4096;
constexpr uint32_t MAX_SNAPSHOTS       = 8192;
constexpr float    PAL_CLOCK           = 985248.0f;
constexpr float    NTSC_CLOCK          = 1022727.0f;

// SID register offsets (from base $D400)
constexpr uint8_t REG_V1_FREQ_LO   = 0x00;
constexpr uint8_t REG_V1_FREQ_HI   = 0x01;
constexpr uint8_t REG_V1_PW_LO     = 0x02;
constexpr uint8_t REG_V1_PW_HI     = 0x03;
constexpr uint8_t REG_V1_CONTROL   = 0x04;
constexpr uint8_t REG_V1_AD        = 0x05;
constexpr uint8_t REG_V1_SR        = 0x06;

constexpr uint8_t REG_V2_FREQ_LO   = 0x07;
constexpr uint8_t REG_V2_FREQ_HI   = 0x08;
constexpr uint8_t REG_V2_PW_LO     = 0x09;
constexpr uint8_t REG_V2_PW_HI     = 0x0A;
constexpr uint8_t REG_V2_CONTROL   = 0x0B;
constexpr uint8_t REG_V2_AD        = 0x0C;
constexpr uint8_t REG_V2_SR        = 0x0D;

constexpr uint8_t REG_V3_FREQ_LO   = 0x0E;
constexpr uint8_t REG_V3_FREQ_HI   = 0x0F;
constexpr uint8_t REG_V3_PW_LO     = 0x10;
constexpr uint8_t REG_V3_PW_HI     = 0x11;
constexpr uint8_t REG_V3_CONTROL   = 0x12;
constexpr uint8_t REG_V3_AD        = 0x13;
constexpr uint8_t REG_V3_SR        = 0x14;

constexpr uint8_t REG_FC_LO        = 0x15;
constexpr uint8_t REG_FC_HI        = 0x16;
constexpr uint8_t REG_RES_FILT     = 0x17;
constexpr uint8_t REG_MODE_VOL     = 0x18;

constexpr uint8_t REG_POTX         = 0x19;
constexpr uint8_t REG_POTY         = 0x1A;
constexpr uint8_t REG_OSC3         = 0x1B;
constexpr uint8_t REG_ENV3         = 0x1C;

// Control register bits
constexpr uint8_t CTRL_GATE        = 0x01;
constexpr uint8_t CTRL_SYNC        = 0x02;
constexpr uint8_t CTRL_RING        = 0x04;
constexpr uint8_t CTRL_TEST        = 0x08;
constexpr uint8_t CTRL_TRIANGLE    = 0x10;
constexpr uint8_t CTRL_SAWTOOTH    = 0x20;
constexpr uint8_t CTRL_PULSE       = 0x40;
constexpr uint8_t CTRL_NOISE       = 0x80;

// ─────────────────────────────────────────────────────────────────────────────
// Script command types
// ─────────────────────────────────────────────────────────────────────────────

enum class cmd_type_t : uint8_t {
    RESET,              // Reset SID
    REVISION,           // Set chip revision
    WRITE,              // Write register
    RUN,                // Run N cycles
    CHECK_OSC3,         // Enable OSC3 checking
    CHECK_ENV3,         // Enable ENV3 checking
    EXPECT_OSC3,        // Assert OSC3 value
    EXPECT_ENV3,        // Assert ENV3 value
    EXPECT_ACC,         // Assert accumulator value
    SNAPSHOT,           // Record current state
    LABEL,              // Named marker
};

// ─────────────────────────────────────────────────────────────────────────────
// Script command
// ─────────────────────────────────────────────────────────────────────────────

struct command_t {
    cmd_type_t type;
    uint8_t    reg;         // Register offset (for WRITE)
    uint8_t    value;       // Value (for WRITE, EXPECT_*)
    uint8_t    voice;       // Voice index (for EXPECT_ACC)
    uint32_t   cycles;      // Cycle count (for RUN)
    uint32_t   acc_value;   // 24-bit accumulator value (for EXPECT_ACC)
    bool       is_8580;     // Revision flag (for REVISION)
    char       label[32];   // Label name (for LABEL)
    int        line_number; // Source line for error reporting
};

// ─────────────────────────────────────────────────────────────────────────────
// Snapshot: recorded state at a point in time
// ─────────────────────────────────────────────────────────────────────────────

struct snapshot_t {
    uint32_t cycle;         // Absolute cycle count
    uint8_t  osc3;          // OSC3 value
    uint8_t  env3;          // ENV3 value
    uint32_t acc[3];        // Voice accumulators
    uint8_t env_amp[3];     // Envelope amplitudes (8-bit)
    char     label[32];     // Optional label
};

// ─────────────────────────────────────────────────────────────────────────────
// Test result entry
// ─────────────────────────────────────────────────────────────────────────────

enum class result_type_t : uint8_t {
    PASS,
    FAIL,
    INFO,
};

struct result_entry_t {
    result_type_t type;
    uint32_t      cycle;        // Cycle at which check occurred
    uint8_t       expected;     // Expected value
    uint8_t       actual;       // Actual value
    const char*   check_name;   // "OSC3", "ENV3", "ACC0" etc.
    int           line_number;  // Source script line
    char          message[128]; // Descriptive message
};

// ─────────────────────────────────────────────────────────────────────────────
// Trace buffer for cycle-by-cycle recording
// ─────────────────────────────────────────────────────────────────────────────

struct trace_entry_t {
    uint8_t osc3;
    uint8_t env3;
};

// ─────────────────────────────────────────────────────────────────────────────
// Test script: a sequence of commands
// ─────────────────────────────────────────────────────────────────────────────

struct test_script_t {
    std::string              name;
    std::string              description;
    std::vector<command_t>   commands;
};

// ─────────────────────────────────────────────────────────────────────────────
// Test harness state
// ─────────────────────────────────────────────────────────────────────────────

struct harness_t {
    // SID chip under test
    mos6581_t*  sid;
    bool        sid_owned;      // True if we allocated it

    // Execution state
    uint32_t    total_cycles;   // Total cycles executed
    bool        check_osc3;     // OSC3 checking active
    bool        check_env3;     // ENV3 checking active

    // Trace recording
    std::vector<trace_entry_t>  trace;
    bool                        trace_enabled;

    // Snapshots
    std::vector<snapshot_t>     snapshots;

    // Results
    std::vector<result_entry_t> results;
    int                         pass_count;
    int                         fail_count;

    // Verbosity
    bool verbose;
};

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

/// Create a new test harness with a fresh SID instance.
harness_t* create(bool verbose = false);

/// Create a test harness wrapping an existing SID (does not take ownership).
harness_t* create_with_sid(mos6581_t* sid, bool verbose = false);

/// Destroy harness (and owned SID if applicable).
void destroy(harness_t* h);

/// Reset harness state and SID.
void reset(harness_t* h);

// ── Script loading ──────────────────────────────────────────────────────────

/// Parse a test script from a string buffer.
/// Returns true on success. On failure, adds error results.
bool parse_script(const char* text, test_script_t* script, const char* name = nullptr);

/// Parse a test script from a file.
bool parse_script_file(const char* path, test_script_t* script);

// ── Programmatic command building ───────────────────────────────────────────

void cmd_reset(test_script_t* s);
void cmd_revision(test_script_t* s, bool is_8580);
void cmd_write(test_script_t* s, uint8_t reg, uint8_t value);
void cmd_run(test_script_t* s, uint32_t cycles);
void cmd_check_osc3(test_script_t* s);
void cmd_check_env3(test_script_t* s);
void cmd_expect_osc3(test_script_t* s, uint8_t value);
void cmd_expect_env3(test_script_t* s, uint8_t value);
void cmd_expect_acc(test_script_t* s, uint8_t voice, uint32_t value);
void cmd_snapshot(test_script_t* s);
void cmd_label(test_script_t* s, const char* label);

// ── Execution ───────────────────────────────────────────────────────────────

/// Execute a test script against the harness. Returns number of failures.
int run_script(harness_t* h, const test_script_t* script);

/// Run a single command.
void execute_command(harness_t* h, const command_t* cmd);

// ── Direct register access (bypasses script) ────────────────────────────────

void write_reg(harness_t* h, uint8_t reg, uint8_t value);
uint8_t read_osc3(harness_t* h);
uint8_t read_env3(harness_t* h);
void clock_cycles(harness_t* h, uint32_t n);

// ── Results ─────────────────────────────────────────────────────────────────

/// Print results summary to stdout.
void print_results(const harness_t* h, const test_script_t* script);

/// Print trace buffer to stdout (compact format).
void print_trace(const harness_t* h, uint32_t start_cycle = 0, uint32_t count = 0);

/// Export trace to binary file (2 bytes per cycle: OSC3, ENV3).
bool export_trace(const harness_t* h, const char* path);

/// Import reference trace from binary file.
bool import_reference_trace(const char* path, std::vector<trace_entry_t>& ref);

/// Compare harness trace against reference trace. Returns mismatch count.
int compare_traces(const harness_t* h, const std::vector<trace_entry_t>& ref,
                   uint32_t start = 0, uint32_t count = 0);

// ── Built-in test suites ────────────────────────────────────────────────────

/// Run all built-in digital verification tests. Returns total failures.
int run_all_builtin_tests(harness_t* h, bool verbose = true);

/// Individual test categories (each returns failure count)
int test_sawtooth_waveform(harness_t* h);
int test_triangle_waveform(harness_t* h);
int test_pulse_waveform(harness_t* h);
int test_noise_waveform(harness_t* h);
int test_combined_waveforms(harness_t* h);
int test_envelope_attack(harness_t* h);
int test_envelope_decay_sustain(harness_t* h);
int test_envelope_release(harness_t* h);
int test_envelope_adsr_bug(harness_t* h);
int test_ring_modulation(harness_t* h);
int test_oscillator_sync(harness_t* h);
int test_test_bit(harness_t* h);
int test_noise_lfsr_sequence(harness_t* h);

// ── reSID behavioral conformance tests ──────────────────────────────────────
int test_resid_rate_counter_15bit(harness_t* h);
int test_resid_sustain_level_change(harness_t* h);
int test_resid_lfsr_reset_value(harness_t* h);
int test_resid_exponential_decay_exact(harness_t* h);
int test_resid_gate_retrigger(harness_t* h);
int test_resid_hold_zero(harness_t* h);

} // namespace sid_test
