#include "fam65xx_cpp_test_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// ============================================================================
// Klaus2m5 Test Runner for fam65xx_cpp Implementation
// ============================================================================

static void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -t, --trace FILE        Enable instruction tracing to FILE\n");
    printf("  -c, --cycles NUM        Set maximum cycles (default: %llu)\n", KLAUS_MAX_CYCLES);
    printf("  -f, --functional        Run only functional test (default)\n");
    printf("  -d, --decimal           Run only decimal test\n");
    printf("  -i, --interrupt         Run only interrupt test\n");
    printf("  -a, --all               Run all tests (default)\n");
    printf("  -v, --verbose           Enable verbose output\n");
    printf("\n");
    printf("Klaus2m5 6502 Functional Test Suite for fam65xx_cpp Implementation\n");
    printf("Tests the new C++ CPU core against comprehensive test vectors.\n");
}

typedef enum {
    TEST_MODE_ALL,
    TEST_MODE_FUNCTIONAL,
    TEST_MODE_DECIMAL,
    TEST_MODE_INTERRUPT
} test_mode_t;

int main(int argc, char* argv[]) {
    // Command line options
    const char* trace_file = NULL;
    uint64_t max_cycles = KLAUS_MAX_CYCLES;
    test_mode_t test_mode = TEST_MODE_ALL;
    bool verbose = false;

    // Parse command line arguments
    static const struct option long_options[] = {
        {"help",       no_argument,       0, 'h'},
        {"trace",      required_argument, 0, 't'},
        {"cycles",     required_argument, 0, 'c'},
        {"functional", no_argument,       0, 'f'},
        {"decimal",    no_argument,       0, 'm'},
        {"interrupt",  no_argument,       0, 'i'},
        {"all",        no_argument,       0, 'a'},
        {"verbose",    no_argument,       0, 'v'},
        {"debug",      no_argument,       0, 'd'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "ht:c:fmiavd", long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 't':
                trace_file = optarg;
                break;
            case 'c':
                max_cycles = strtoull(optarg, NULL, 0);
                break;
            case 'f':
                test_mode = TEST_MODE_FUNCTIONAL;
                break;
            case 'm':
                test_mode = TEST_MODE_DECIMAL;
                break;
            case 'i':
                test_mode = TEST_MODE_INTERRUPT;
                break;
            case 'a':
                test_mode = TEST_MODE_ALL;
                break;
            case 'v':
            case 'd':
                verbose = true;
                break;
            case '?':
                fprintf(stderr, "Unknown option. Use -h for help.\n");
                return 1;
            default:
                break;
        }
    }

    // Banner
    printf("========================================\n");
    printf("    Klaus2m5 6502 Test Suite Runner\n");
    printf("    Testing fam65xx_cpp Implementation\n");
    printf("========================================\n");

    if (verbose) {
        printf("Configuration:\n");
        printf("  Max cycles: %lu\n", max_cycles);
        printf("  Trace file: %s\n", trace_file ? trace_file : "disabled");
        printf("  Test mode: ");
        switch (test_mode) {
            case TEST_MODE_ALL:        printf("all tests\n"); break;
            case TEST_MODE_FUNCTIONAL: printf("functional only\n"); break;
            case TEST_MODE_DECIMAL:    printf("decimal only\n"); break;
            case TEST_MODE_INTERRUPT:  printf("interrupt only\n"); break;
        }
        printf("\n");
    }

    bool all_passed = true;

    // Run tests based on mode
    switch (test_mode) {
        case TEST_MODE_ALL:
            all_passed = fam65xx_klaus_test_suite_run_all();
            break;

        case TEST_MODE_FUNCTIONAL: {
            fam65xx_test_harness_t* harness = fam65xx_test_harness_create();
            if (!harness) {
                fprintf(stderr, "Failed to create test harness\n");
                return 1;
            }

            // Configure trace if requested
            if (trace_file) {
                fam65xx_test_harness_enable_trace(harness, trace_file);
            }

            // Load and run functional test
            char test_path[512];
            snprintf(test_path, sizeof(test_path),
                     "/home/patrick/Git/aiemu/external/6502-tests/6502_65C02_functional_tests/bin_files/6502_functional_test.bin");

            if (fam65xx_test_harness_load_binary(harness, test_path)) {
                harness->max_cycles = max_cycles;
                fam65xx_test_status_t status = fam65xx_test_harness_run_klaus_test(harness);
                fam65xx_test_harness_print_status(&status);
                all_passed = (status.result == TEST_PASSED);
            } else {
                all_passed = false;
            }

            fam65xx_test_harness_destroy(harness);
            break;
        }

        case TEST_MODE_DECIMAL: {
            fam65xx_test_harness_t* harness = fam65xx_test_harness_create();
            if (!harness) {
                fprintf(stderr, "Failed to create test harness\n");
                return 1;
            }

            // Configure trace if requested
            if (trace_file) {
                fam65xx_test_harness_enable_trace(harness, trace_file);
            }

            // Load and run 65C02 extended opcodes test
            char test_path[512];
            snprintf(test_path, sizeof(test_path),
                     "/home/patrick/Git/aiemu/external/6502-tests/6502_65C02_functional_tests/bin_files/65C02_extended_opcodes_test.bin");

            if (fam65xx_test_harness_load_binary(harness, test_path)) {
                harness->max_cycles = max_cycles;
                fam65xx_test_status_t status = fam65xx_test_harness_run_klaus_test(harness);
                fam65xx_test_harness_print_status(&status);
                all_passed = (status.result == TEST_PASSED);
            } else {
                all_passed = false;
            }

            fam65xx_test_harness_destroy(harness);
            break;
        }

        case TEST_MODE_INTERRUPT:
            printf("Interrupt test mode requires assembly from source\n");
            printf("6502_interrupt_test.a65 source available but not assembled\n");
            all_passed = false;
            break;
    }

    // Final result
    printf("\n========================================\n");
    if (all_passed) {
        printf("🎉 ALL TESTS PASSED! 🎉\n");
        printf("Your fam65xx_cpp implementation is working correctly!\n");
        printf("========================================\n");
        return 0;
    } else {
        printf("❌ SOME TESTS FAILED\n");
        printf("Please check your fam65xx_cpp implementation.\n");
        printf("========================================\n");
        return 1;
    }
}