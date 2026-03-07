// =============================================================================
// Atari 2600 Test Runner — Unified test executable
// =============================================================================
// Runs all built-in Atari 2600 hardware verification tests.
//
// Tests cover:
//   - TIA: registers, collision, playfield, players, missiles, ball,
//          HMOVE, WSYNC, VSYNC, VBLANK, colors, audio, input, VDEL
//   - RIOT: 128-byte RAM, all 4 timer modes, underflow, DDR, port masking
//   - Mappers: 2K, 4K, F8, F6, F4, E0, 3F, FA, factory detection
//   - System: address decoding, CPU-TIA sync timing, frame cycle count
//
// Usage:
//   a2600_test_runner                    # Run all built-in tests
//   a2600_test_runner --verbose          # Verbose output
//
// Exit code: 0 = all tests passed, non-zero = failure count
// =============================================================================

#include "../src/testing/a2600_test_harness.h"
#include <cstring>
#include <cstdio>

#ifndef CERMU_IMPL
#define CERMU_IMPL
#endif

static void print_usage(const char* argv0) {
    printf("Atari 2600 Hardware Verification Test Runner\n\n");
    printf("Usage: %s [options]\n\n", argv0);
    printf("Options:\n");
    printf("  --verbose    Show all test results (including passes)\n");
    printf("  --help       Show this help message\n\n");
}

int main(int argc, char* argv[]) {
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    a2600_test::harness_t* h = a2600_test::create(verbose);
    if (!h) {
        printf("ERROR: Failed to create A2600 test harness\n");
        return 1;
    }

    int failures = a2600_test::run_all_builtin_tests(h, verbose);

    a2600_test::destroy(h);

    return failures;
}
