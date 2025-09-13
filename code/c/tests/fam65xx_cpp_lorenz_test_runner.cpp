#include "fam65xx_cpp_lorenz_test_harness.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <getopt.h>

using namespace fam65xx_cpp;

struct TestInfo {
    std::string name;
    std::string description;
    std::string binary_file;
    std::string expected_file;
};

// Wolfgang Lorenz test suite definitions
std::vector<TestInfo> get_lorenz_tests() {
    return {
        {"ladx", "LDA/LDX addressing mode tests", "ladx", "ladx.exp"},
        {"sbcb", "SBC decimal mode edge cases", "sbcb", "sbcb.exp"},
        {"cpxy", "CPX/CPY comparison tests", "cpxy", "cpxy.exp"},
        {"cmpx", "CMP instruction comprehensive testing", "cmpx", "cmpx.exp"},
        {"branchwrap", "Branch page boundary behavior", "branchwrap", "branchwrap.exp"},
        {"mmufetch", "Memory fetch cycle timing", "mmufetch", "mmufetch.exp"},
        {"trap", "Illegal opcode behavior", "trap", "trap.exp"},
        {"flag", "Processor status flag edge cases", "flag", "flag.exp"},
        {"cia1tab", "CIA timer A interaction", "cia1tab", "cia1tab.exp"},
        {"cia2tab", "CIA timer B behavior", "cia2tab", "cia2tab.exp"},
        {"irq", "IRQ timing validation", "irq", "irq.exp"},
        {"nmi", "NMI edge detection timing", "nmi", "nmi.exp"}
    };
}

void print_usage(const char* program_name) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Wolfgang Lorenz 6502 Test Runner" << std::endl;
    std::cout << "  Testing fam65xx_cpp Implementation" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nUsage: " << program_name << " [options] [test_name]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  -h, --help           Show this help message" << std::endl;
    std::cout << "  -l, --list           List available tests" << std::endl;
    std::cout << "  -a, --all            Run all available tests" << std::endl;
    std::cout << "  -v, --verbose        Enable verbose output" << std::endl;
    std::cout << "  -t, --trace FILE     Enable instruction tracing to file" << std::endl;
    std::cout << "  -c, --cycles NUM     Set maximum cycle count (default: 1000000)" << std::endl;
    std::cout << "  -o, --output FILE    Save results to file" << std::endl;
    std::cout << "\nTest Names:" << std::endl;
    
    auto tests = get_lorenz_tests();
    for (const auto& test : tests) {
        std::cout << "  " << std::left << std::setw(12) << test.name 
                  << " - " << test.description << std::endl;
    }
    
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program_name << " ladx                    # Run LDA/LDX tests" << std::endl;
    std::cout << "  " << program_name << " -a -v                   # Run all tests with verbose output" << std::endl;
    std::cout << "  " << program_name << " -t trace.log sbcb       # Run SBC tests with tracing" << std::endl;
    std::cout << std::endl;
}

void list_tests() {
    std::cout << "\nAvailable Wolfgang Lorenz Tests:" << std::endl;
    std::cout << "=================================" << std::endl;
    
    auto tests = get_lorenz_tests();
    for (const auto& test : tests) {
        // Check if test file exists
        std::string test_path = "../external/lorenz-tests/bin/" + test.binary_file;
        bool exists = std::filesystem::exists(test_path);
        
        std::cout << std::left << std::setw(12) << test.name 
                  << " - " << test.description 
                  << (exists ? " [AVAILABLE]" : " [MISSING]") << std::endl;
    }
    std::cout << std::endl;
}

bool run_single_test(const TestInfo& test, bool verbose, const std::string& trace_file, 
                    uint64_t max_cycles, const std::string& output_file) {
    
    if (verbose) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Running Test: " << test.name << std::endl;
        std::cout << "Description: " << test.description << std::endl;
        std::cout << "========================================" << std::endl;
    }
    
    // Check if test file exists
    std::string test_path = "../external/lorenz-tests/bin/" + test.binary_file;
    if (!std::filesystem::exists(test_path)) {
        std::cout << "ERROR: Test file not found: " << test_path << std::endl;
        std::cout << "Please download Wolfgang Lorenz test files to external/lorenz-tests/bin/" << std::endl;
        return false;
    }
    
    LorenzTestHarness harness;
    
    // Configure tracing if requested
    if (!trace_file.empty()) {
        harness.enable_trace(trace_file);
        if (verbose) {
            std::cout << "Tracing enabled: " << trace_file << std::endl;
        }
    }
    
    // Load test
    if (!harness.load_test(test.binary_file)) {
        std::cout << "ERROR: Failed to load test: " << test.binary_file << std::endl;
        return false;
    }
    
    // Load expected results if available
    harness.load_expected_results(test.expected_file);
    
    if (verbose) {
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Max cycles: " << max_cycles << std::endl;
        std::cout << "  Trace file: " << (trace_file.empty() ? "disabled" : trace_file) << std::endl;
        std::cout << "  Test file: " << test.binary_file << std::endl;
        std::cout << std::endl;
    }
    
    // Run test
    std::cout << "Starting Wolfgang Lorenz test: " << test.name << "..." << std::endl;
    
    LorenzTestResult result = harness.run_test(max_cycles);
    
    // Save results if requested
    if (!output_file.empty()) {
        if (save_test_results(output_file, result)) {
            if (verbose) {
                std::cout << "Results saved to: " << output_file << std::endl;
            }
        } else {
            std::cout << "Warning: Failed to save results to: " << output_file << std::endl;
        }
    }
    
    // Print result summary
    bool passed = (result.status == LorenzTestResult::Status::PASSED);
    
    std::cout << "\n========================================" << std::endl;
    if (passed) {
        std::cout << "🎉 TEST PASSED! 🎉" << std::endl;
        std::cout << "Wolfgang Lorenz test '" << test.name << "' completed successfully!" << std::endl;
    } else {
        std::cout << "❌ TEST FAILED ❌" << std::endl;
        std::cout << "Wolfgang Lorenz test '" << test.name << "' failed." << std::endl;
        if (!result.message.empty()) {
            std::cout << "Reason: " << result.message << std::endl;
        }
    }
    std::cout << "========================================" << std::endl;
    
    return passed;
}

int main(int argc, char* argv[]) {
    // Command line options
    bool verbose = false;
    bool list_only = false;
    bool run_all = false;
    std::string trace_file;
    std::string output_file;
    uint64_t max_cycles = 1000000;
    std::string test_name;
    
    // Parse command line options
    static struct option long_options[] = {
        {"help",    no_argument,       0, 'h'},
        {"list",    no_argument,       0, 'l'},
        {"all",     no_argument,       0, 'a'},
        {"verbose", no_argument,       0, 'v'},
        {"trace",   required_argument, 0, 't'},
        {"cycles",  required_argument, 0, 'c'},
        {"output",  required_argument, 0, 'o'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hlavt:c:o:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'l':
                list_only = true;
                break;
            case 'a':
                run_all = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 't':
                trace_file = optarg;
                break;
            case 'c':
                max_cycles = std::stoull(optarg);
                break;
            case 'o':
                output_file = optarg;
                break;
            case '?':
                return 1;
            default:
                abort();
        }
    }
    
    // Get remaining arguments (test name)
    if (optind < argc) {
        test_name = argv[optind];
    }
    
    // Handle list option
    if (list_only) {
        list_tests();
        return 0;
    }
    
    // Get available tests
    auto tests = get_lorenz_tests();
    
    if (run_all) {
        // Run all tests
        std::cout << "\n========================================" << std::endl;
        std::cout << "Running All Wolfgang Lorenz Tests" << std::endl;
        std::cout << "========================================" << std::endl;
        
        int passed_count = 0;
        int total_count = tests.size();
        
        for (const auto& test : tests) {
            std::string test_output = output_file.empty() ? "" : test.name + "_" + output_file;
            std::string test_trace = trace_file.empty() ? "" : test.name + "_" + trace_file;
            
            if (run_single_test(test, verbose, test_trace, max_cycles, test_output)) {
                passed_count++;
            }
            
            std::cout << std::endl;
        }
        
        // Final summary
        std::cout << "\n========================================" << std::endl;
        std::cout << "Wolfgang Lorenz Test Suite Results" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Tests passed: " << passed_count << "/" << total_count << std::endl;
        
        if (passed_count == total_count) {
            std::cout << "\n🎉 ALL TESTS PASSED! 🎉" << std::endl;
            std::cout << "Your fam65xx_cpp implementation passes the Wolfgang Lorenz test suite!" << std::endl;
        } else {
            std::cout << "\n❌ SOME TESTS FAILED ❌" << std::endl;
            std::cout << "Further debugging is needed to achieve full compliance." << std::endl;
        }
        std::cout << "========================================" << std::endl;
        
        return (passed_count == total_count) ? 0 : 1;
        
    } else if (!test_name.empty()) {
        // Run specific test
        auto test_it = std::find_if(tests.begin(), tests.end(),
            [&test_name](const TestInfo& t) { return t.name == test_name; });
        
        if (test_it == tests.end()) {
            std::cout << "Error: Unknown test name: " << test_name << std::endl;
            std::cout << "Use --list to see available tests." << std::endl;
            return 1;
        }
        
        bool passed = run_single_test(*test_it, verbose, trace_file, max_cycles, output_file);
        return passed ? 0 : 1;
        
    } else {
        // No test specified
        print_usage(argv[0]);
        return 1;
    }
}