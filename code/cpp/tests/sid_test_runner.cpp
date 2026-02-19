// =============================================================================
// SID Test Runner — Unified test executable
// =============================================================================
// Runs all built-in SID digital verification tests and optionally loads
// test scripts from files or compares against reference trace data.
//
// Usage:
//   sid_test_runner                    # Run all built-in tests
//   sid_test_runner --verbose          # Verbose output (show passes too)
//   sid_test_runner --script <file>    # Run a test script file
//   sid_test_runner --trace <file>     # Export trace to file
//   sid_test_runner --ref <file>       # Compare against reference trace
//   sid_test_runner --revision 8580    # Use 8580 chip revision
//
// Exit code: 0 = all tests passed, non-zero = failure count
// =============================================================================

#include "../src/testing/sid_test_harness.h"
#include <cstring>
#include <cstdio>

// Implement the chip.cpp dependency minimally if not linking full core
#ifndef CERMU_IMPL
#define CERMU_IMPL
#endif

static void print_usage(const char* argv0) {
    printf("SID MOS6581/8580 Digital Verification Test Runner\n\n");
    printf("Usage: %s [options]\n\n", argv0);
    printf("Options:\n");
    printf("  --verbose          Show all test results (including passes)\n");
    printf("  --script <file>    Run a test script file (.sid_test format)\n");
    printf("  --trace <file>     Export execution trace to binary file\n");
    printf("  --ref <file>       Compare against reference trace file\n");
    printf("  --revision <type>  Set chip revision: 6581 (default) or 8580\n");
    printf("  --help             Show this help message\n\n");
    printf("Script format (one command per line, # for comments):\n");
    printf("  reset                        Reset SID\n");
    printf("  revision, 6581|8580          Set chip revision\n");
    printf("  write, <reg>, <value>        Write to SID register (hex/dec)\n");
    printf("  run, <cycles>                Run N PHI2 cycles\n");
    printf("  check_osc3                   Enable OSC3 trace recording\n");
    printf("  check_env3                   Enable ENV3 trace recording\n");
    printf("  expect_osc3, <value>         Assert OSC3 == value\n");
    printf("  expect_env3, <value>         Assert ENV3 == value\n");
    printf("  expect_acc, <voice>, <value> Assert accumulator value\n");
    printf("  snapshot                     Record current state\n");
    printf("  label, <name>                Named marker\n");
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool use_8580 = false;
    const char* script_path = nullptr;
    const char* trace_path = nullptr;
    const char* ref_path = nullptr;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            script_path = argv[++i];
        } else if (strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (strcmp(argv[i], "--ref") == 0 && i + 1 < argc) {
            ref_path = argv[++i];
        } else if (strcmp(argv[i], "--revision") == 0 && i + 1 < argc) {
            i++;
            use_8580 = (strcmp(argv[i], "8580") == 0);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Create test harness
    sid_test::harness_t* h = sid_test::create(verbose);
    if (!h) {
        printf("ERROR: Failed to create SID test harness\n");
        return 1;
    }

    // Set revision if requested
    if (use_8580) {
        mos6581_set_revision(h->sid, SID_REVISION_8580_R5);
        printf("Using 8580 revision\n");
    }

    int failures = 0;

    if (script_path) {
        // Run a specific script file
        printf("Loading script: %s\n", script_path);
        sid_test::test_script_t script;
        if (sid_test::parse_script_file(script_path, &script)) {
            // Enable tracing if we're going to export
            if (trace_path || ref_path) {
                h->trace_enabled = true;
            }

            failures = sid_test::run_script(h, &script);
            sid_test::print_results(h, &script);

            // Export trace if requested
            if (trace_path) {
                if (sid_test::export_trace(h, trace_path)) {
                    printf("Trace exported to: %s (%zu cycles)\n",
                           trace_path, h->trace.size());
                } else {
                    printf("ERROR: Failed to export trace to %s\n", trace_path);
                }
            }

            // Compare against reference if requested
            if (ref_path) {
                std::vector<sid_test::trace_entry_t> ref;
                if (sid_test::import_reference_trace(ref_path, ref)) {
                    int mismatches = sid_test::compare_traces(h, ref);
                    printf("Reference comparison: %d mismatches out of %zu cycles\n",
                           mismatches, std::min(h->trace.size(), ref.size()));
                    failures += mismatches;
                } else {
                    printf("ERROR: Failed to load reference trace from %s\n", ref_path);
                    failures++;
                }
            }
        } else {
            printf("ERROR: Failed to parse script: %s\n", script_path);
            failures = 1;
        }
    } else {
        // Run all built-in tests
        failures = sid_test::run_all_builtin_tests(h, verbose);
    }

    sid_test::destroy(h);

    return failures;
}
