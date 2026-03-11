#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <cstdint>
#include "../core/system_lines.h"  // bus_state_t, BUS_GET_ADDR, BUS_GET_DATA

// Forward declarations
class RAMChip;
class C64System;

namespace c64_test {

// Test result status
enum class TestStatus {
    NOT_RUN,
    PASSED,
    FAILED,
    TIMEOUT,
    SKIPPED,
    ERROR
};
// Test types
enum class TestType {
    EXITCODE,      // Tests that use debug register $D7FF (0=pass, 0xFF=fail)
    SCREENSHOT,    // Tests that require visual comparison with reference image
    INTERACTIVE,   // Tests requiring user interaction (skipped in batch mode)
    ANALYZER       // Tests using logic analyzer output
};

// Test completion protocols
enum class TestProtocol {
    DEBUG_REGISTER,    // Uses $D7FF: 0x00=pass, 0xFF=fail
    INFINITE_LOOP,     // Enters infinite loop at specific PC when done
    BASIC_LOADER,      // Requires BASIC execution to calculate entry point
    KERNAL_EXIT,       // Returns to KERNAL (e.g., RTS to specific address)
    AUTO_DETECT        // Auto-detect protocol from test structure
};

// Test execution environment
enum class TestEnvironment {
    AUTO_DETECT,          // Automatically detect required environment
    DIRECT_EXECUTION,     // Load and execute directly (no KERNAL/BASIC)
    BASIC_BOOT,           // Full KERNAL+BASIC boot, then execute
    KERNAL_BOOT           // KERNAL boot only (no BASIC)
};

// Hardware configuration flags
enum class HardwareConfig {
    NONE = 0,
    CIA_OLD = 1 << 0,
    CIA_NEW = 1 << 1,
    SID_6581 = 1 << 2,
    SID_8580 = 1 << 3,
    VICII_PAL = 1 << 4,
    VICII_NTSC = 1 << 5,
    VICII_NTSCOLD = 1 << 6,
    VICII_DREAN = 1 << 7,
    VICII_OLD = 1 << 8,
    VICII_NEW = 1 << 9
};

inline HardwareConfig operator|(HardwareConfig a, HardwareConfig b) {
    return static_cast<HardwareConfig>(static_cast<int>(a) | static_cast<int>(b));
}

inline HardwareConfig operator&(HardwareConfig a, HardwareConfig b) {
    return static_cast<HardwareConfig>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool operator!(HardwareConfig a) {
    return static_cast<int>(a) == 0;
}

// Individual test descriptor
struct TestDescriptor {
    std::string path;              // Path to test file relative to VICE-testprogs
    std::string name;              // Test name (filename)
    TestType type;                 // Test type
    TestProtocol protocol;         // Completion detection protocol
    TestEnvironment environment;   // Execution environment requirement
    uint32_t timeout_cycles;       // Timeout in CPU cycles (default 10 million)
    HardwareConfig required_hw;    // Required hardware configuration
    HardwareConfig excluded_hw;    // Hardware configurations to skip
    std::string reference_image;   // Path to reference screenshot (if applicable)
    std::string description;       // Test description
    std::string category;          // Category (CPU, CIA, VICII, etc.)
    uint16_t success_pc;           // Expected PC for INFINITE_LOOP protocol (0=unknown)
    uint16_t failure_pc;           // Expected PC for failure in INFINITE_LOOP protocol (0=unknown)
    
    TestDescriptor()
        : type(TestType::EXITCODE)
        , protocol(TestProtocol::AUTO_DETECT)
        , environment(TestEnvironment::AUTO_DETECT)
        , timeout_cycles(50000000)
        , required_hw(HardwareConfig::NONE)
        , excluded_hw(HardwareConfig::NONE)
        , success_pc(0)
        , failure_pc(0)
    {}
};

// Test result
struct TestResult {
    TestDescriptor test;
    TestStatus status;
    std::string message;           // Error/status message
    uint32_t cycles_executed;      // Cycles executed before completion/timeout
    uint8_t exit_code;             // Exit code from debug register
    std::string screenshot_path;   // Path to generated screenshot (if applicable)
    double execution_time_ms;      // Wall clock execution time
    
    TestResult() 
        : status(TestStatus::NOT_RUN)
        , cycles_executed(0)
        , exit_code(0xFF)
        , execution_time_ms(0.0)
    {}
};

// Test suite for organizing related tests
struct TestSuite {
    std::string name;
    std::string description;
    std::vector<TestDescriptor> tests;
};

// Test filter for selecting tests to run
struct TestFilter {
    std::string path_filter;       // Substring filter for test paths
    std::vector<std::string> categories;  // Filter by category
    HardwareConfig hardware;       // Current hardware configuration
    bool skip_interactive;         // Skip interactive tests
    bool skip_screenshots;         // Skip screenshot tests
    
    TestFilter()
        : hardware(HardwareConfig::NONE)
        , skip_interactive(true)
        , skip_screenshots(false)
    {}
    
    bool matches(const TestDescriptor& test) const;
};

// =============================================================================
// IO Write Intercept - wraps a chip write callback to capture specific writes
// =============================================================================
// This allows the test framework to monitor writes to specific IO addresses
// (e.g., $D7FF debug register) WITHOUT adding any overhead to the normal
// emulator hot path. The interceptor is installed by patching the io_handlers
// array on the bus, and removed when testing completes.
//
// Usage pattern for other addresses: create additional io_write_intercept_t
// instances and install them on the appropriate IO page(s).
struct io_write_intercept_t {
    bus_state_t (*original_write_handler)(void* context, bus_state_t bus_state);
    void* original_chip_instance;
    uint16_t watch_address;       // Full 16-bit address to intercept (e.g., 0xD7FF)
    volatile bool written;        // Set true when watch_address is written
    uint8_t value;                // Value that was written to watch_address
};

// Generic interceptor: checks address, captures value, passes through to original
bus_state_t io_write_intercept_handler(void* context, bus_state_t bus_state);

// Test framework class
class TestFramework {
public:
    TestFramework(const std::string& vice_testprogs_path);
    ~TestFramework();
    
    // Test discovery
    bool scan_tests();
    bool load_test_list(const std::string& testlist_file);
    std::vector<TestDescriptor> get_all_tests() const;
    std::vector<TestDescriptor> get_filtered_tests(const TestFilter& filter) const;
    
    // Test execution with automatic hardware reconfiguration
    TestResult run_test(const TestDescriptor& test, C64System* c64);
    TestResult run_test_safe(const TestDescriptor& test, C64System* c64);  // SEH-protected on Windows
    std::vector<TestResult> run_tests(const std::vector<TestDescriptor>& tests, C64System* c64);
    std::vector<TestResult> run_all_tests(C64System* c64, const TestFilter& filter);
    
    // New: Test execution with dynamic system creation per test
    TestResult run_test_with_config(const TestDescriptor& test);
    std::vector<TestResult> run_tests_with_auto_config(const std::vector<TestDescriptor>& tests);
    
    // Result management
    void save_results(const std::string& output_file, const std::vector<TestResult>& results);
    void save_results_json(const std::string& output_file, const std::vector<TestResult>& results);
    void save_results_html(const std::string& output_file, const std::vector<TestResult>& results);
    std::vector<TestResult> load_previous_results(const std::string& results_file);
    
    // Get failed tests from previous run for re-testing
    std::vector<TestDescriptor> get_failed_tests(const std::vector<TestResult>& results);
    
    // Statistics
    struct TestStats {
        int total;
        int passed;
        int failed;
        int timeout;
        int skipped;
        int error;
        double total_time_ms;
        
        double pass_rate() const {
            return total > 0 ? (100.0 * passed / total) : 0.0;
        }
    };
    
    TestStats get_statistics(const std::vector<TestResult>& results) const;
    void print_statistics(const std::vector<TestResult>& results) const;
    void print_summary(const std::vector<TestResult>& results) const;
    
    // Configuration
    void set_vice_testprogs_path(const std::string& path) { vice_testprogs_path_ = path; }
    void set_output_directory(const std::string& path) { output_directory_ = path; }
    void set_verbose(bool verbose) { verbose_ = verbose; }
    
    // Hardware configuration management
    bool requires_reconfiguration(const TestDescriptor& test) const;
    C64System* create_system_for_test(const TestDescriptor& test);
    HardwareConfig get_current_hardware_config() const { return current_hardware_; }
    
private:
    std::string vice_testprogs_path_;
    std::string output_directory_;
    bool verbose_;
    
    std::vector<TestSuite> test_suites_;
    std::map<std::string, TestDescriptor> test_registry_;
    
    // Hardware configuration tracking
    HardwareConfig current_hardware_;
    bool is_pal_system_;
    bool is_ntsc_system_;
    
    // Helper methods
    bool discover_tests_in_directory(const std::string& category_path, const std::string& category);
    TestDescriptor parse_test_from_file(const std::string& prg_path, const std::string& category);
    bool check_for_reference_image(const std::string& prg_path, TestDescriptor& test);
    std::string find_readme(const std::string& directory);
    
    // Environment detection
    TestEnvironment detect_test_environment(const TestDescriptor& test, uint16_t load_addr, uint16_t sys_addr);
    
    // Protocol detection and handling
    TestProtocol detect_test_protocol(const TestDescriptor& test, C64System* c64);
    bool detect_basic_two_stage_loader(RAMChip* ram, uint16_t load_addr);
    uint16_t calculate_basic_entry_point(RAMChip* ram, uint16_t sys_addr);
    bool detect_infinite_loop(C64System* c64, uint16_t& loop_pc, uint32_t check_cycles = 1000);
    uint8_t get_border_color(C64System* c64);
    
    // Test execution helpers
    bool load_test_program(const TestDescriptor& test, C64System* c64);
    bool execute_basic_boot(C64System* c64, const TestDescriptor& test, uint16_t sys_addr);
    bool execute_kernal_boot(C64System* c64);
    TestResult run_exitcode_test(const TestDescriptor& test, C64System* c64);
    TestResult run_exitcode_test_enhanced(const TestDescriptor& test, C64System* c64);
    TestResult run_screenshot_test(const TestDescriptor& test, C64System* c64);
    bool compare_screenshots(const std::string& generated, const std::string& reference, double& similarity);
    
    // Debug register monitoring via IO write intercept (zero-cost to main emulator)
    static constexpr uint16_t DEBUG_REGISTER = 0xD7FF;
    static constexpr uint8_t  DEBUG_REGISTER_IO_PAGE = 7; // IO page for $D700-$D7FF
    io_write_intercept_t debug_intercept_;  // Intercept state for $D7FF
    bool debug_intercept_installed_;        // Whether intercept is currently active

    // Install/uninstall the $D7FF write interceptor on the bus io_handlers
    void install_debug_intercept(C64System* c64);
    void uninstall_debug_intercept(C64System* c64);
};

// Utility functions
std::string test_status_to_string(TestStatus status);
std::string test_type_to_string(TestType type);
std::string hardware_config_to_string(HardwareConfig config);
HardwareConfig parse_hardware_config(const std::string& config_str);

} // namespace c64_test