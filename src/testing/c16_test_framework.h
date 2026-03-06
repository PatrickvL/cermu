#pragma once

// =============================================================================
// C16/Plus4 Test Framework — Automated testing for TED 7360-based systems
// =============================================================================
// Modelled after c64_test_framework.h but adapted for the C264 series:
//   - Debug register at $FDCF (not $D7FF)
//   - BASIC start at $1001 (not $0801)
//   - Border color via TED register $FF19 (7-bit: lum[6:4] | hue[3:0])
//   - TED color hue indices: 2=red, 5=green (matching VICE testbench convention)
//   - Supports Plus4 (64KB), C16 (16KB), C116 (16KB)
// =============================================================================

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace c16_test {

// ---- Test result status (shared with C64 framework) ----
enum class TestStatus {
    NOT_RUN,
    PASSED,
    FAILED,
    TIMEOUT,
    SKIPPED,
    ERROR
};

// ---- Test types ----
enum class TestType {
    EXITCODE,      // Uses debug register $FDCF (0=pass, 0xFF=fail)
    SCREENSHOT,    // Visual comparison with reference image
    INTERACTIVE    // Requires user interaction (skipped in batch mode)
};

// ---- Hardware configuration flags ----
enum class HardwareConfig : uint32_t {
    NONE       = 0,
    TED_PAL    = 1 << 0,
    TED_NTSC   = 1 << 1,
    RAM_16K    = 1 << 2,   // C16/C116
    RAM_64K    = 1 << 3,   // Plus/4
};

inline HardwareConfig operator|(HardwareConfig a, HardwareConfig b) {
    return static_cast<HardwareConfig>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline HardwareConfig operator&(HardwareConfig a, HardwareConfig b) {
    return static_cast<HardwareConfig>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool has_flag(HardwareConfig config, HardwareConfig flag) {
    return static_cast<uint32_t>(config & flag) != 0;
}

// ---- Individual test descriptor ----
struct TestDescriptor {
    std::string path;              // Path to test file relative to VICE-testprogs root
    std::string name;              // Filename (e.g., "outrun.prg")
    TestType    type;              // Test type
    uint32_t    timeout_cycles;    // Timeout in CPU cycles
    HardwareConfig required_hw;    // Required hardware config
    std::string reference_image;   // Path to reference screenshot (if applicable)
    std::string category;          // Category (TED, Plus4, selftest, demos)
    std::string description;       // Human-readable test description
    bool        expect_fail;       // True if test is expected to fail (e.g., selftest/plus4-fail.prg)

    TestDescriptor()
        : type(TestType::EXITCODE)
        , timeout_cycles(50000000)     // ~56 seconds of Plus/4 PAL time
        , required_hw(HardwareConfig::NONE)
        , expect_fail(false)
    {}
};

// ---- Test result ----
struct TestResult {
    TestDescriptor test;
    TestStatus  status;
    std::string message;           // Status/error message
    uint32_t    cycles_executed;   // Cycles before completion/timeout
    uint8_t     exit_code;         // Value written to $FDCF
    double      execution_time_ms; // Wall clock time

    TestResult()
        : status(TestStatus::NOT_RUN)
        , cycles_executed(0)
        , exit_code(0xFF)
        , execution_time_ms(0.0)
    {}
};

// ---- Test filter ----
struct TestFilter {
    std::string path_filter;                // Substring match on test path
    std::vector<std::string> categories;    // Filter by category
    HardwareConfig hardware;                // Current hardware configuration
    bool skip_interactive;                  // Skip interactive tests
    bool skip_screenshots;                  // Skip screenshot tests

    TestFilter()
        : hardware(HardwareConfig::NONE)
        , skip_interactive(true)
        , skip_screenshots(false)
    {}

    bool matches(const TestDescriptor& test) const;
};

// ---- Aggregate statistics ----
struct TestStats {
    int total    = 0;
    int passed   = 0;
    int failed   = 0;
    int timeout  = 0;
    int skipped  = 0;
    int error    = 0;
    double total_time_ms = 0.0;

    double pass_rate() const {
        return total > 0 ? (100.0 * passed / total) : 0.0;
    }
};

// =============================================================================
// TestFramework — manages test discovery, execution, and reporting
// =============================================================================
class TestFramework {
public:
    explicit TestFramework(const std::string& vice_testprogs_path);
    ~TestFramework();

    // ---- Test discovery ----
    bool scan_tests();
    std::vector<TestDescriptor> get_all_tests() const;
    std::vector<TestDescriptor> get_filtered_tests(const TestFilter& filter) const;

    // ---- Test execution (creates/destroys system per test) ----
    TestResult run_test(const TestDescriptor& test);
    std::vector<TestResult> run_all_tests(const TestFilter& filter);

    // ---- Result management ----
    void save_results(const std::string& output_file, const std::vector<TestResult>& results);
    void save_results_json(const std::string& output_file, const std::vector<TestResult>& results);

    // ---- Statistics ----
    TestStats get_statistics(const std::vector<TestResult>& results) const;
    void print_summary(const std::vector<TestResult>& results) const;

    // ---- Configuration ----
    void set_verbose(bool v) { verbose_ = v; }
    void set_output_directory(const std::string& dir) { output_directory_ = dir; }

private:
    std::string vice_testprogs_path_;
    std::string output_directory_;
    bool verbose_;

    std::map<std::string, TestDescriptor> test_registry_;

    // Discovery helpers
    bool discover_tests_in_directory(const std::string& dir_path,
                                     const std::string& category,
                                     const std::string& rel_prefix);
    TestDescriptor parse_test_from_file(const std::string& full_path,
                                        const std::string& category);
    void register_testlist_tests();
};

// Utility functions
std::string test_status_to_string(TestStatus status);
std::string test_type_to_string(TestType type);

} // namespace c16_test
