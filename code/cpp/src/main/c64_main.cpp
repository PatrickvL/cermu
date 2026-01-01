#include "../systems/c64/c64.h"
#include "../systems/c64/c64_config.h"
#include "../systems/c64/c64_test_framework.h"
#include <iostream>
#include <cstdint>
#include <cstring>

// Forward declarations for banking verification
extern void verify_c64_banking_modes();
extern void verify_c64_banking_by_table();

// ============================================================================
// MAIN FUNCTION - Modern C++ test harness
// ============================================================================
int main(int argc, char** argv) {
    // Parse command-line arguments
    bool run_banking_verify = false;
    bool banking_verify_all = false;
    bool run_tests = false;
    bool scan_tests = false;
    const char* test_path = nullptr;
    const char* test_category = nullptr;
    const char* test_filter = nullptr;
    const char* test_output = nullptr;
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--verify-banking") == 0) {
            run_banking_verify = true;
        } else if (std::strcmp(argv[i], "--verify-banking-all") == 0) {
            run_banking_verify = true;
            banking_verify_all = true;
        } else if (std::strcmp(argv[i], "--test-run") == 0 && i + 1 < argc) {
            run_tests = true;
            test_path = argv[++i];
        } else if (std::strcmp(argv[i], "--test-scan") == 0 && i + 1 < argc) {
            scan_tests = true;
            test_path = argv[++i];
        } else if (std::strcmp(argv[i], "--test-category") == 0 && i + 1 < argc) {
            test_category = argv[++i];
        } else if (std::strcmp(argv[i], "--test-filter") == 0 && i + 1 < argc) {
            test_filter = argv[++i];
        } else if (std::strcmp(argv[i], "--test-output") == 0 && i + 1 < argc) {
            test_output = argv[++i];
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        }
    }
    
    // Run banking verification if requested (standalone mode)
    if (run_banking_verify) {
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════════════════\n";
        std::cout << "  C64 Banking Mode Verification (Standalone)\n";
        std::cout << "═══════════════════════════════════════════════════════════════════\n";
        std::cout.flush();
        
        if (banking_verify_all) {
            std::cout << "Running verify_c64_banking_modes()...\n";
            std::cout.flush();
            verify_c64_banking_modes();
        } else {
            std::cout << "Running verify_c64_banking_by_table()...\n";
            std::cout.flush();
            verify_c64_banking_by_table();
        }
        std::cout << "\nVerification complete.\n";
        std::cout.flush();
        return 0;  // Exit after verification (no emulator run)
    }
    
    // Run test framework if requested
    if (scan_tests || run_tests) {
        if (!test_path) {
            std::cerr << "Error: Test path required (--test-scan or --test-run <path>)" << std::endl;
            return 1;
        }
        
        c64_test::TestFramework framework(test_path);
        framework.set_verbose(verbose);
        
        if (scan_tests) {
            std::cout << "Scanning tests..." << std::endl;
            if (!framework.scan_tests()) {
                std::cerr << "Failed to scan tests" << std::endl;
                return 1;
            }
            
            auto all_tests = framework.get_all_tests();
            std::cout << "Found " << all_tests.size() << " tests" << std::endl;
            return 0;
        }
        
        if (run_tests) {
            std::cout << "Scanning tests..." << std::endl;
            if (!framework.scan_tests()) {
                std::cerr << "Failed to scan tests" << std::endl;
                return 1;
            }
            
            // Build filter
            c64_test::TestFilter filter;
            filter.skip_interactive = true;
            filter.hardware = c64_test::HardwareConfig::VICII_PAL;  // Default to PAL
            
            if (test_category) {
                filter.categories.push_back(test_category);
            }
            if (test_filter) {
                filter.path_filter = test_filter;
            }
            
            // Get filtered tests
            auto tests = framework.get_filtered_tests(filter);
            std::cout << "Running " << tests.size() << " tests..." << std::endl;
            
            // Run tests with auto-config
            auto results = framework.run_tests_with_auto_config(tests);
            
            // Print summary
            framework.print_summary(results);
            
            // Save results if requested
            if (test_output) {
                framework.save_results_json(test_output, results);
            }
            
            return 0;
        }
    }
    
    try {
        // Create system configuration (PAL by default) using C++ initialization
        c64_config_t config{
            .vicii_standard = VIC_PAL,
            .rom_config = nullptr,  // Use default ROM paths
            .roml_present = false,
            .romh_present = false,
            .roml_filename = nullptr,
            .romh_filename = nullptr,
            .initial_exrom_state = true,   // Default EXROM high (inactive)
            .initial_game_state = true     // Default GAME high (inactive)
        };
        
        // Initialize the C64 system and get the instance
        c64_t* c64 = c64_system_create(&config);
        if (!c64) {
            std::cerr << "Failed to initialize C64 system!" << std::endl;
            return 1;
        }

        // Simple run loop - run enough cycles to complete boot sequence
        std::cout << "C64 emulator initialized. Running 1000000 cycles..." << std::endl;
        constexpr std::uint64_t cycles = 1000000;
   
        for (std::uint64_t i = 0; i < cycles; ++i) {
            c64_system_tick(c64);
        }
        
        std::cout << "Completed " << cycles << " cycles." << std::endl;

        // Clean up
        c64_system_destroy(c64);
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred!" << std::endl;
        return 1;
    }
}