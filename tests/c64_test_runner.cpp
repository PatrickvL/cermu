#include "../src/testing/c64_test_framework.h"
#include "../src/systems/commodore/c64/c64_system.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_usage(const char* prog_name) {
    printf("C64 Test Runner - Automated testing tool for C64 emulator\n\n");
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  --testprogs <path>     Path to VICE-testprogs directory (required)\n");
    printf("  --filter <substring>   Filter tests by path substring\n");
    printf("  --category <name>      Filter tests by category (CPU, CIA, VICII, VIC20, etc.)\n");
    printf("  --output <file>        Output results to file (default: test_results.txt)\n");
    printf("  --json <file>          Output results as JSON\n");
    printf("  --failed <file>        Re-run only failed tests from previous results\n");
    printf("  --verbose              Enable verbose output\n");
    printf("  --legacy-mode          Use single hardware configuration (disables auto-config)\n");
    printf("  --pal                  Filter for PAL-only tests (with --legacy-mode: set PAL mode)\n");
    printf("  --ntsc                 Filter for NTSC-only tests (with --legacy-mode: set NTSC mode)\n");
    printf("  --cia-old              Filter for old CIA tests\n");
    printf("  --cia-new              Filter for new CIA tests\n");
    printf("  --list                 List all tests without running\n");
    printf("  --help                 Show this help message\n\n");
    printf("Hardware Configuration:\n");
    printf("  By default, tests run with automatic hardware reconfiguration.\n");
    printf("  The framework creates appropriate C64/VIC20 systems per test requirements.\n");
    printf("  Use --legacy-mode to disable this and use a single fixed configuration.\n\n");
    printf("Examples:\n");
    printf("  %s --testprogs /path/to/VICE-testprogs\n", prog_name);
    printf("  %s --testprogs /path/to/VICE-testprogs --filter CPU/kdormann\n", prog_name);
    printf("  %s --testprogs /path/to/VICE-testprogs --category VICII\n", prog_name);
    printf("  %s --testprogs /path/to/VICE-testprogs --category VIC20\n", prog_name);
    printf("  %s --testprogs /path/to/VICE-testprogs --failed previous_results.json\n", prog_name);
    printf("  %s --testprogs /path/to/VICE-testprogs --legacy-mode --pal\n", prog_name);
}

int main(int argc, char** argv) {
    // Disable stdout buffering for crash diagnostics
    setvbuf(stdout, NULL, _IONBF, 0);
    
#ifdef _WIN32
    // Disable CRT error popups and Windows Error Reporting to let SEH handle crashes
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    
    // Parse command line arguments
    const char* testprogs_path = nullptr;
    const char* output_file = "test_results.txt";
    const char* json_file = nullptr;
    const char* failed_file = nullptr;
    bool verbose = false;
    bool list_only = false;
    bool auto_config = true;  // Default to automatic hardware reconfiguration
    bool legacy_mode = false;  // Disable auto-config if user wants single system
    
    c64_test::TestFilter filter;
    filter.skip_interactive = true;
    filter.skip_screenshots = false;
    
    // Parse arguments
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
        } else if (strcmp(argv[i], "--failed") == 0 && i + 1 < argc) {
            failed_file = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--legacy-mode") == 0) {
            legacy_mode = true;
            auto_config = false;
        } else if (strcmp(argv[i], "--list") == 0) {
            list_only = true;
        } else if (strcmp(argv[i], "--pal") == 0) {
            filter.hardware = filter.hardware | c64_test::HardwareConfig::VICII_PAL;
        } else if (strcmp(argv[i], "--ntsc") == 0) {
            filter.hardware = filter.hardware | c64_test::HardwareConfig::VICII_NTSC;
        } else if (strcmp(argv[i], "--cia-old") == 0) {
            filter.hardware = filter.hardware | c64_test::HardwareConfig::CIA_OLD;
        } else if (strcmp(argv[i], "--cia-new") == 0) {
            filter.hardware = filter.hardware | c64_test::HardwareConfig::CIA_NEW;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate required arguments
    if (!testprogs_path) {
        printf("ERROR: --testprogs path is required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Create test framework
    c64_test::TestFramework framework(testprogs_path);
    framework.set_verbose(verbose);
    
    // Scan for tests
    printf("Scanning for tests...\n");
    if (!framework.scan_tests()) {
        printf("ERROR: Failed to scan tests\n");
        return 1;
    }
    
    // Get filtered test list
    std::vector<c64_test::TestDescriptor> tests;
    
    if (failed_file) {
        // Re-run failed tests from previous run
        printf("Loading previous results from: %s\n", failed_file);
        auto previous_results = framework.load_previous_results(failed_file);
        tests = framework.get_failed_tests(previous_results);
        printf("Found %zu failed tests to re-run\n", tests.size());
    } else {
        tests = framework.get_filtered_tests(filter);
    }
    
    if (tests.empty()) {
        printf("No tests match the specified filters\n");
        return 0;
    }
    
    printf("Selected %zu tests\n", tests.size());
    
    // List tests if requested
    if (list_only) {
        printf("\nTest List:\n");
        printf("==========\n");
        for (const auto& test : tests) {
            printf("  [%s] %s\n", test.category.c_str(), test.path.c_str());
            if (test.type == c64_test::TestType::SCREENSHOT) {
                printf("    Type: Screenshot test\n");
            }
            if (static_cast<int>(test.required_hw) != 0) {
                printf("    Requires: %s\n", 
                       c64_test::hardware_config_to_string(test.required_hw).c_str());
            }
        }
        return 0;
    }
    
    // Run tests with appropriate method
    std::vector<c64_test::TestResult> results;
    
    if (auto_config) {
        // Use automatic hardware reconfiguration (default, recommended)
        printf("\nRunning tests with automatic hardware reconfiguration...\n");
        results = framework.run_tests_with_auto_config(tests);
    } else {
        // Use single system configuration (legacy mode for compatibility)
        printf("\nLegacy mode: Initializing single C64 system...\n");
        
        // Default to PAL, adjust based on hardware filter
        int region_index = 0;  // PAL
        if (static_cast<int>(filter.hardware & c64_test::HardwareConfig::VICII_NTSC)) {
            region_index = 1;  // NTSC
        }
        
        C64System* c64 = new C64System();
        auto cfg = c64->get_configuration();
        cfg.region_option_index = region_index;
        c64->set_configuration(cfg);
        if (!c64->initialize()) {
            printf("ERROR: Failed to create C64 system\n");
            return 1;
        }
        
        // Run tests
        printf("\n");
        results = framework.run_tests(tests, c64);
        
        // Cleanup
        c64->shutdown();
        delete c64;
    }
    
    // Print summary
    printf("\n");
    framework.print_summary(results);
    
    // Save results
    printf("\n");
    framework.save_results(output_file, results);
    
    if (json_file) {
        framework.save_results_json(json_file, results);
    }
    
    // Return non-zero if there were failures
    auto stats = framework.get_statistics(results);
    return (stats.failed > 0 || stats.timeout > 0 || stats.error > 0) ? 1 : 0;
}