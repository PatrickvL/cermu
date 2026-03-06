// =============================================================================
// C16/Plus4 Test Runner — CLI entry point for VICE-testprogs Plus4/TED tests
// =============================================================================
// Usage:
//   c16_test_runner --testprogs /path/to/VICE-testprogs [options]
//
// Follows the same CLI pattern as c64_test_runner.
// =============================================================================

#include "../src/testing/c16_test_framework.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static void print_usage(const char* prog) {
    printf("C16/Plus4 Test Runner — Automated testing for TED 7360-based systems\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --testprogs <path>     Path to VICE-testprogs directory (required)\n");
    printf("  --filter <substring>   Filter tests by path substring\n");
    printf("  --category <name>      Filter by category (TED, Plus4, selftest)\n");
    printf("  --output <file>        Output results to file (default: c16_test_results.txt)\n");
    printf("  --json <file>          Output results as JSON\n");
    printf("  --verbose              Enable verbose output\n");
    printf("  --list                 List all tests without running\n");
    printf("  --pal                  Filter for PAL-only tests\n");
    printf("  --ntsc                 Filter for NTSC-only tests\n");
    printf("  --help                 Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s --testprogs /path/to/VICE-testprogs\n", prog);
    printf("  %s --testprogs /path/to/VICE-testprogs --filter selftest\n", prog);
    printf("  %s --testprogs /path/to/VICE-testprogs --category TED --verbose\n", prog);
}

int main(int argc, char** argv) {
    // Disable stdout buffering for crash diagnostics
    setvbuf(stdout, nullptr, _IONBF, 0);

    // Parse command line arguments
    const char* testprogs_path = nullptr;
    const char* output_file = "c16_test_results.txt";
    const char* json_file = nullptr;
    bool verbose = false;
    bool list_only = false;

    c16_test::TestFilter filter;
    filter.skip_interactive = true;
    filter.skip_screenshots = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--testprogs") == 0 && i + 1 < argc) {
            testprogs_path = argv[++i];
        } else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            filter.path_filter = argv[++i];
        } else if (strcmp(argv[i], "--category") == 0 && i + 1 < argc) {
            filter.categories.push_back(argv[++i]);
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0 && i + 1 < argc) {
            json_file = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--list") == 0) {
            list_only = true;
        } else if (strcmp(argv[i], "--pal") == 0) {
            filter.hardware = filter.hardware | c16_test::HardwareConfig::TED_PAL;
        } else if (strcmp(argv[i], "--ntsc") == 0) {
            filter.hardware = filter.hardware | c16_test::HardwareConfig::TED_NTSC;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!testprogs_path) {
        printf("ERROR: --testprogs path is required\n\n");
        print_usage(argv[0]);
        return 1;
    }

    // Create test framework
    c16_test::TestFramework framework(testprogs_path);
    framework.set_verbose(verbose);

    // Scan for tests
    printf("Scanning for Plus4/TED tests...\n");
    if (!framework.scan_tests()) {
        printf("ERROR: No tests found\n");
        return 1;
    }

    // Get filtered test list
    auto tests = framework.get_filtered_tests(filter);

    if (tests.empty()) {
        printf("No tests match the specified filters\n");
        return 0;
    }

    printf("Selected %zu tests\n", tests.size());

    // List mode
    if (list_only) {
        printf("\nTest List:\n==========\n");
        for (const auto& t : tests) {
            printf("  [%s] %s", t.category.c_str(), t.path.c_str());
            if (t.expect_fail) printf(" (expect fail)");
            if (t.type == c16_test::TestType::SCREENSHOT) printf(" [screenshot]");
            printf("\n");
        }
        return 0;
    }

    // Run tests
    auto results = framework.run_all_tests(filter);

    // Print summary
    printf("\n");
    framework.print_summary(results);

    // Save results
    printf("\n");
    framework.save_results(output_file, results);
    if (json_file)
        framework.save_results_json(json_file, results);

    // Return non-zero on failures
    auto stats = framework.get_statistics(results);
    return (stats.failed > 0 || stats.timeout > 0 || stats.error > 0) ? 1 : 0;
}
