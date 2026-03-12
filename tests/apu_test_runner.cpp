// =============================================================================
// NES APU Test Runner — Unified test executable
// =============================================================================
// Runs all built-in NES APU digital verification tests.
//
// Usage:
//   apu_test_runner                    # Run all built-in tests
//   apu_test_runner --verbose          # Verbose output
//
// Exit code: 0 = all tests passed, non-zero = failure count
// =============================================================================

#include "../src/testing/apu_test_harness.hpp"
#include <cstring>
#include <cstdio>

#ifndef CERMU_IMPL
#define CERMU_IMPL
#endif

static void print_usage(const char* argv0) {
    printf("NES APU (2A03) Digital Verification Test Runner\n\n");
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

    apu_test::harness_t* h = apu_test::create(verbose);
    if (!h) {
        printf("ERROR: Failed to create APU test harness\n");
        return 1;
    }

    int failures = apu_test::run_all_builtin_tests(h, verbose);

    apu_test::destroy(h);

    return failures;
}
