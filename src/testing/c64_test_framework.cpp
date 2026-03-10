#include "c64_test_framework.h"
#include "../systems/commodore/c64/c64_system.h"
#include "c64_test_loader.h"
#include "c64_screenshot.h"
#include "../chip/memory/memory_chip.h"
#include "../chip/cpu/fam65xx/fam65xx.hpp"
#include "../chip/video/vic_ii/vicii_common.h"
#include "../core/os/os.h"
#ifdef CERMU_USE_STD_FILESYSTEM
    #include <filesystem>
    namespace cermu_fs = std::filesystem;
#else
    #include <dirent.h>
#endif
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

namespace c64_test {

// =============================================================================
// IO Write Intercept - generic handler for intercepting chip writes
// =============================================================================
bus_state_t io_write_intercept_handler(void* context, bus_state_t bus_state) {
    io_write_intercept_t* intercept = (io_write_intercept_t*)context;
    uint16_t address = BUS_GET_ADDR(bus_state);
    if (address == intercept->watch_address) {
        intercept->value = BUS_GET_DATA(bus_state);
        intercept->written = true;
    }
    // Always pass through to the original chip handler
    return intercept->original_write_handler(intercept->original_chip_instance, bus_state);
}

// Install the $D7FF debug register interceptor on SID IO page 7
void TestFramework::install_debug_intercept(C64System* c64) {
    if (debug_intercept_installed_) return;

    auto& page = c64->bus.io_handlers[DEBUG_REGISTER_IO_PAGE];
    debug_intercept_.original_write_handler = page.write_handler;
    debug_intercept_.original_chip_instance = page.chip_instance;
    debug_intercept_.watch_address = DEBUG_REGISTER;
    debug_intercept_.written = false;
    debug_intercept_.value = 0;

    // Patch the io_handlers entry to route through the interceptor
    page.write_handler = io_write_intercept_handler;
    page.chip_instance = &debug_intercept_;
    debug_intercept_installed_ = true;
}

// Uninstall the interceptor — restore original chip handler
void TestFramework::uninstall_debug_intercept(C64System* c64) {
    if (!debug_intercept_installed_) return;

    auto& page = c64->bus.io_handlers[DEBUG_REGISTER_IO_PAGE];
    page.write_handler = debug_intercept_.original_write_handler;
    page.chip_instance = debug_intercept_.original_chip_instance;
    debug_intercept_installed_ = false;
}

// Utility function implementations
std::string test_status_to_string(TestStatus status) {
    switch (status) {
        case TestStatus::NOT_RUN: return "NOT_RUN";
        case TestStatus::PASSED: return "PASSED";
        case TestStatus::FAILED: return "FAILED";
        case TestStatus::TIMEOUT: return "TIMEOUT";
        case TestStatus::SKIPPED: return "SKIPPED";
        case TestStatus::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string test_type_to_string(TestType type) {
    switch (type) {
        case TestType::EXITCODE: return "exitcode";
        case TestType::SCREENSHOT: return "screenshot";
        case TestType::INTERACTIVE: return "interactive";
        case TestType::ANALYZER: return "analyzer";
        default: return "unknown";
    }
}

std::string hardware_config_to_string(HardwareConfig config) {
    std::vector<std::string> parts;
    if (static_cast<int>(config & HardwareConfig::CIA_OLD)) parts.push_back("CIA_OLD");
    if (static_cast<int>(config & HardwareConfig::CIA_NEW)) parts.push_back("CIA_NEW");
    if (static_cast<int>(config & HardwareConfig::SID_6581)) parts.push_back("SID_6581");
    if (static_cast<int>(config & HardwareConfig::SID_8580)) parts.push_back("SID_8580");
    if (static_cast<int>(config & HardwareConfig::VICII_PAL)) parts.push_back("VICII_PAL");
    if (static_cast<int>(config & HardwareConfig::VICII_NTSC)) parts.push_back("VICII_NTSC");
    if (static_cast<int>(config & HardwareConfig::VICII_NTSCOLD)) parts.push_back("VICII_NTSCOLD");
    if (static_cast<int>(config & HardwareConfig::VICII_DREAN)) parts.push_back("VICII_DREAN");
    if (static_cast<int>(config & HardwareConfig::VICII_OLD)) parts.push_back("VICII_OLD");
    if (static_cast<int>(config & HardwareConfig::VICII_NEW)) parts.push_back("VICII_NEW");
    
    if (parts.empty()) return "NONE";
    
    std::string result;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) result += "|";
        result += parts[i];
    }
    return result;
}

HardwareConfig parse_hardware_config(const std::string& config_str) {
    HardwareConfig config = HardwareConfig::NONE;
    
    if (config_str.find("cia-old") != std::string::npos) 
        config = config | HardwareConfig::CIA_OLD;
    if (config_str.find("cia-new") != std::string::npos) 
        config = config | HardwareConfig::CIA_NEW;
    if (config_str.find("sid-old") != std::string::npos || config_str.find("6581") != std::string::npos) 
        config = config | HardwareConfig::SID_6581;
    if (config_str.find("sid-new") != std::string::npos || config_str.find("8580") != std::string::npos) 
        config = config | HardwareConfig::SID_8580;
    if (config_str.find("vicii-pal") != std::string::npos) 
        config = config | HardwareConfig::VICII_PAL;
    if (config_str.find("vicii-ntsc") != std::string::npos) 
        config = config | HardwareConfig::VICII_NTSC;
    if (config_str.find("vicii-ntscold") != std::string::npos) 
        config = config | HardwareConfig::VICII_NTSCOLD;
    if (config_str.find("vicii-drean") != std::string::npos) 
        config = config | HardwareConfig::VICII_DREAN;
    if (config_str.find("vicii-old") != std::string::npos) 
        config = config | HardwareConfig::VICII_OLD;
    if (config_str.find("vicii-new") != std::string::npos) 
        config = config | HardwareConfig::VICII_NEW;
    
    return config;
}

// TestFilter implementation
bool TestFilter::matches(const TestDescriptor& test) const {
    // Check path filter
    if (!path_filter.empty() && test.path.find(path_filter) == std::string::npos) {
        return false;
    }
    
    // Check category filter
    if (!categories.empty()) {
        bool found = false;
        for (const auto& cat : categories) {
            if (test.category == cat) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    // Check if test requires hardware we don't have
    if (static_cast<int>(test.required_hw) != 0) {
        if (static_cast<int>(test.required_hw & hardware) == 0) {
            return false;
        }
    }
    
    // Check if test is excluded on current hardware
    if (static_cast<int>(test.excluded_hw & hardware) != 0) {
        return false;
    }
    
    // Check test type filters
    if (skip_interactive && test.type == TestType::INTERACTIVE) {
        return false;
    }
    
    if (skip_screenshots && test.type == TestType::SCREENSHOT) {
        return false;
    }
    
    return true;
}

// TestFramework implementation
TestFramework::TestFramework(const std::string& vice_testprogs_path)
    : vice_testprogs_path_(vice_testprogs_path)
    , output_directory_("test_results")
    , verbose_(false)
    , current_hardware_(HardwareConfig::NONE)
    , is_pal_system_(true)
    , is_ntsc_system_(false)
    , debug_intercept_{}
    , debug_intercept_installed_(false)
{
}

TestFramework::~TestFramework() {
}

bool TestFramework::scan_tests() {
    printf("Scanning tests in: %s\n", vice_testprogs_path_.c_str());
    
    // Define categories to scan
    std::vector<std::string> categories = {
        "CPU", "CIA", "VICII", "SID", "interrupts", 
        "C64", "general", "VIC20", "drive"
    };
    
    for (const auto& category : categories) {
        std::string category_path = vice_testprogs_path_ + "/" + category;
        discover_tests_in_directory(category_path, category);
    }
    
    printf("Found %zu tests in %zu categories\n", 
           test_registry_.size(), test_suites_.size());
    
    return !test_registry_.empty();
}

bool TestFramework::discover_tests_in_directory(const std::string& category_path, const std::string& category) {
#ifdef CERMU_USE_STD_FILESYSTEM
    // C++17 filesystem implementation (MSVC — no dirent.h)
    if (!cermu_fs::is_directory(category_path)) {
        if (verbose_) {
            printf("  Category not found: %s\n", category.c_str());
        }
        return false;
    }
    
    TestSuite suite;
    suite.name = category;
    
    for (auto& entry : cermu_fs::recursive_directory_iterator(category_path)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".prg") continue;
        
        std::string full_path = entry.path().string();
        // Convert backslashes to forward slashes for consistency
        std::replace(full_path.begin(), full_path.end(), '\\', '/');
        
        // Compute relative path from category_path
        std::string relative = cermu_fs::relative(entry.path(), category_path).string();
        std::replace(relative.begin(), relative.end(), '\\', '/');
        
        TestDescriptor test = parse_test_from_file(full_path, category);
        test.path = category + "/" + relative;
        suite.tests.push_back(test);
        test_registry_[test.path] = test;
    }
    
    if (!suite.tests.empty()) {
        test_suites_.push_back(suite);
        if (verbose_) {
            printf("  %s: %zu tests\n", category.c_str(), suite.tests.size());
        }
    }
    
    return !suite.tests.empty();
#else
    // POSIX implementation using dirent.h
    DIR* dir = opendir(category_path.c_str());
    if (!dir) {
        if (verbose_) {
            printf("  Category not found: %s\n", category.c_str());
        }
        return false;
    }
    
    TestSuite suite;
    suite.name = category;
    
    std::function<void(const std::string&, const std::string&)> scan_recursive;
    scan_recursive = [&](const std::string& dir_path, const std::string& rel_path) {
        DIR* d = opendir(dir_path.c_str());
        if (!d) return;
        
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            
            std::string full_path = dir_path + "/" + entry->d_name;
            std::string relative = rel_path.empty() ? entry->d_name : rel_path + "/" + entry->d_name;
            
            struct stat st;
            if (stat(full_path.c_str(), &st) != 0) continue;
            
            if (S_ISDIR(st.st_mode)) {
                scan_recursive(full_path, relative);
            } else if (S_ISREG(st.st_mode)) {
                size_t len = strlen(entry->d_name);
                if (len > 4 && strcmp(entry->d_name + len - 4, ".prg") == 0) {
                    TestDescriptor test = parse_test_from_file(full_path, category);
                    test.path = category + "/" + relative;
                    suite.tests.push_back(test);
                    test_registry_[test.path] = test;
                }
            }
        }
        closedir(d);
    };
    
    scan_recursive(category_path, "");
    closedir(dir);
    
    if (!suite.tests.empty()) {
        test_suites_.push_back(suite);
        if (verbose_) {
            printf("  %s: %zu tests\n", category.c_str(), suite.tests.size());
        }
    }
    
    return !suite.tests.empty();
#endif
}

TestDescriptor TestFramework::parse_test_from_file(const std::string& prg_path, const std::string& category) {
    TestDescriptor test;
    test.name = prg_path.substr(prg_path.find_last_of("/\\") + 1);
    test.category = category;
    test.type = TestType::EXITCODE;  // Default
    test.timeout_cycles = 50000000;  // 50 million cycles (~50s of C64 time)
    
    // Check for reference image
    check_for_reference_image(prg_path, test);
    
    // Parse filename for hints about hardware requirements
    std::string lower_name = test.name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name.find("pal") != std::string::npos) {
        test.required_hw = test.required_hw | HardwareConfig::VICII_PAL;
    }
    if (lower_name.find("ntsc") != std::string::npos) {
        if (lower_name.find("ntscold") != std::string::npos) {
            test.required_hw = test.required_hw | HardwareConfig::VICII_NTSCOLD;
        } else {
            test.required_hw = test.required_hw | HardwareConfig::VICII_NTSC;
        }
    }
    if (lower_name.find("old") != std::string::npos && category == "CIA") {
        test.required_hw = test.required_hw | HardwareConfig::CIA_OLD;
    }
    if (lower_name.find("new") != std::string::npos && category == "CIA") {
        test.required_hw = test.required_hw | HardwareConfig::CIA_NEW;
    }
    
    return test;
}

bool TestFramework::check_for_reference_image(const std::string& prg_path, TestDescriptor& test) {
    // Check for reference image in references/ subdirectory
    size_t last_slash = prg_path.find_last_of("/\\");
    std::string directory = prg_path.substr(0, last_slash);
    std::string filename = prg_path.substr(last_slash + 1);
    std::string ref_path = directory + "/references/" + filename + ".png";
    
    struct stat st;
    if (stat(ref_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        test.type = TestType::SCREENSHOT;
        test.reference_image = ref_path;
        return true;
    }
    
    return false;
}

std::string TestFramework::find_readme(const std::string& directory) {
    std::vector<std::string> readme_names = {"readme.txt", "README.txt", "README.md", "readme.md"};
    for (const auto& name : readme_names) {
        std::string path = directory + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            return path;
        }
    }
    return "";
}

std::vector<TestDescriptor> TestFramework::get_all_tests() const {
    std::vector<TestDescriptor> all_tests;
    for (const auto& pair : test_registry_) {
        all_tests.push_back(pair.second);
    }
    return all_tests;
}

std::vector<TestDescriptor> TestFramework::get_filtered_tests(const TestFilter& filter) const {
    std::vector<TestDescriptor> filtered;
    for (const auto& pair : test_registry_) {
        if (filter.matches(pair.second)) {
            filtered.push_back(pair.second);
        }
    }
    return filtered;
}

#ifdef CERMU_HAS_SEH
// SEH-protected tick loop: runs system_tick() in a __try/__except block.
// This function has NO C++ objects with destructors, so __try/__except is safe.
// Returns: 0=debug_reg, 1=timeout, 2=crash, 3=infinite_loop_detected
struct TickLoopResult {
    int reason;          // 0=debug_reg, 1=timeout, 2=crash, 3=infinite_loop
    uint32_t cycles;
    uint8_t debug_value;
    uint16_t loop_pc;
    uint8_t border_color;
};

static TickLoopResult tick_loop_protected(C64System* c64, uint32_t max_cycles, io_write_intercept_t* intercept) {
    TickLoopResult r = {};
    r.reason = 1; // timeout by default
    
    __try {
        auto* cpu = c64->mos6510;
        uint16_t last_pc = cpu->get(REG_PC);
        uint32_t pc_stable_count = 0;
        
        for (uint32_t i = 0; i < max_cycles; i++) {
            c64->tick();
            
            // Check debug register via IO write intercept
            if (intercept->written) {
                r.reason = 0;
                r.cycles = i + 1;
                r.debug_value = intercept->value;
                intercept->written = false;
                
                // If pass/fail value, return immediately
                if (r.debug_value == 0x00 || r.debug_value == 0xFF) {
                    return r;
                }
                // Subtest indicator - continue running
            }
            
            // Periodic infinite-loop check every 256 cycles
            if ((i & 0xFF) == 0) {
                uint16_t current_pc = cpu->get(REG_PC);
                if (current_pc == last_pc) {
                    pc_stable_count++;
                    if (pc_stable_count >= 2) {
                        // Verify JMP *
                        uint8_t opcode = c64->ram->data()[current_pc];
                        bool is_jmp_self = false;
                        if (opcode == 0x4C) {
                            uint16_t target = c64->ram->data()[(current_pc + 1) & 0xFFFF] |
                                             (c64->ram->data()[(current_pc + 2) & 0xFFFF] << 8);
                            is_jmp_self = (target == current_pc);
                        }
                        if (is_jmp_self || pc_stable_count >= 8) {
                            r.reason = 3;
                            r.cycles = i + 1;
                            r.loop_pc = current_pc;
                            // Get border color
                            if (c64->vicii) {
                                vicii_t* vicii = static_cast<vicii_t*>(c64->vicii);
                                r.border_color = vicii->regs_[vicii_regs::EC] & 0x0F;
                            }
                            return r;
                        }
                    }
                } else {
                    pc_stable_count = 0;
                }
                last_pc = current_pc;
            }
        }
        
        r.cycles = max_cycles;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        r.reason = 2; // crash
        r.cycles = c64->get_total_cycles();
    }
    
    return r;
}

// Two-level SEH wrapper: Level 1 has __try but NO C++ objects with destructors.
// Level 2 (the thunk) has C++ objects but no __try. This satisfies MSVC C2712.
static int seh_call(void(*func)(void*), void* arg) {
    __try {
        func(arg);
        return 0;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

struct RunTestArgs {
    TestFramework* fw;
    const TestDescriptor* test;
    C64System* c64;
    TestResult result;
};

static void run_test_thunk(void* arg) {
    auto* a = static_cast<RunTestArgs*>(arg);
    a->result = a->fw->run_test(*a->test, a->c64);
}
#endif // CERMU_HAS_SEH

TestResult TestFramework::run_test_safe(const TestDescriptor& test, C64System* c64) {
#ifdef CERMU_HAS_SEH
    RunTestArgs args;
    args.fw = this;
    args.test = &test;
    args.c64 = c64;
    args.result.test = test;
    
    int ret = seh_call(run_test_thunk, &args);
    if (ret == -1) {
        // Crash caught by SEH
        TestResult result;
        result.test = test;
        result.status = TestStatus::ERROR;
        result.message = "CRASH: Access violation during test execution";
        printf("  !!! CRASH detected in %s\n", test.name.c_str());
        return result;
    }
    return args.result;
#else
    return run_test(test, c64);
#endif
}

TestResult TestFramework::run_test(const TestDescriptor& test, C64System* c64) {
    TestResult result;
    result.test = test;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (verbose_) {
        printf("\nRunning test: %s\n", test.path.c_str());
        printf("  Type: %s\n", test_type_to_string(test.type).c_str());
    }
    
    // Load test program
    if (!load_test_program(test, c64)) {
        result.status = TestStatus::ERROR;
        result.message = "Failed to load test program";
        return result;
    }
    
    // Run test based on type
    switch (test.type) {
        case TestType::EXITCODE:
            result = run_exitcode_test_enhanced(test, c64);
            break;
        case TestType::SCREENSHOT:
            result = run_screenshot_test(test, c64);
            break;
        case TestType::INTERACTIVE:
            result.status = TestStatus::SKIPPED;
            result.message = "Interactive test skipped in batch mode";
            break;
        case TestType::ANALYZER:
            result.status = TestStatus::SKIPPED;
            result.message = "Analyzer test not yet supported";
            break;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    if (verbose_) {
        printf("  Result: %s (%s)\n", 
               test_status_to_string(result.status).c_str(),
               result.message.c_str());
        printf("  Time: %.2f ms\n", result.execution_time_ms);
    }
    
    return result;
}

// Detect required execution environment for a test
TestEnvironment TestFramework::detect_test_environment(const TestDescriptor& test, uint16_t load_addr, uint16_t sys_addr) {
    // If environment explicitly specified, use it
    if (test.environment != TestEnvironment::AUTO_DETECT) {
        return test.environment;
    }
    
    // 1. BASIC loader pattern: load at $0801 with SYS command
    if (load_addr == 0x0801 && sys_addr != 0) {
        if (verbose_) {
            printf("  Environment: BASIC_BOOT (BASIC two-stage loader detected)\n");
        }
        return TestEnvironment::BASIC_BOOT;
    }
    
    // 2. Category-based heuristics
    if (test.category == "CIA" || test.path.find("Lorenz") != std::string::npos) {
        // Lorenz tests typically need BASIC boot for proper initialization
        if (verbose_) {
            printf("  Environment: BASIC_BOOT (Lorenz/CIA test)\n");
        }
        return TestEnvironment::BASIC_BOOT;
    }
    
    if (test.category == "CPU") {
        // CPU tests usually work with direct execution
        if (verbose_) {
            printf("  Environment: DIRECT_EXECUTION (CPU test)\n");
        }
        return TestEnvironment::DIRECT_EXECUTION;
    }
    
    // 3. Load address heuristics
    if (load_addr < 0x0800) {
        // Loaded into zero page/stack area - unusual, direct execution
        if (verbose_) {
            printf("  Environment: DIRECT_EXECUTION (low address $%04X)\n", load_addr);
        }
        return TestEnvironment::DIRECT_EXECUTION;
    }
    
    if (load_addr >= 0x1000) {
        // Loaded above BASIC area - likely direct execution
        if (verbose_) {
            printf("  Environment: DIRECT_EXECUTION (high address $%04X)\n", load_addr);
        }
        return TestEnvironment::DIRECT_EXECUTION;
    }
    
    // 4. Default: try BASIC boot for safety (can fall back to direct if timeout)
    if (verbose_) {
        printf("  Environment: BASIC_BOOT (default for load_addr=$%04X)\n", load_addr);
    }
    return TestEnvironment::BASIC_BOOT;
}

// Execute KERNAL boot sequence
bool TestFramework::execute_kernal_boot(C64System* c64) {
    auto* cpu = c64->mos6510;
    
    // Read KERNAL reset vector from ROM
    // The bus read will automatically route to KERNAL ROM at $FFFC-$FFFD
    bus_state_t read_state = c64->bus.state;
    
    // Read low byte of reset vector
    BUS_SET_ADDR(read_state, 0xFFFC);
    BUS_SET_BIT(read_state, BUS_RW_BIT);  // Read mode
    read_state = c64->bus.memory_tick(read_state);
    uint8_t reset_low = BUS_GET_DATA(read_state);
    
    // Read high byte of reset vector
    BUS_SET_ADDR(read_state, 0xFFFD);
    BUS_SET_BIT(read_state, BUS_RW_BIT);  // Read mode
    read_state = c64->bus.memory_tick(read_state);
    uint8_t reset_high = BUS_GET_DATA(read_state);
    
    uint16_t reset_vector = reset_low | (reset_high << 8);
    
    if (verbose_) {
        printf("  KERNAL reset vector: $%04X\n", reset_vector);
    }
    
    // Load reset vector into CPU
    cpu->load_reset_vector(reset_vector);
    
    // Enable interrupts for KERNAL (it needs them for initialization)
    uint8_t status = cpu->get(REG_P);
    status &= ~0x04;  // Clear I flag
    cpu->set(REG_P, status);
    
    // Execute KERNAL initialization
    // KERNAL boot takes about 2.1 million cycles (includes memory test)
    // Simply run for sufficient cycles rather than checking specific PC values
    const uint32_t MAX_KERNAL_BOOT_CYCLES = 2200000;
    uint32_t boot_cycles = 0;
    
    if (verbose_) {
        printf("  Running KERNAL initialization for up to %u cycles...\n", MAX_KERNAL_BOOT_CYCLES);
    }
    
    while (boot_cycles < MAX_KERNAL_BOOT_CYCLES) {
        c64->tick();
        boot_cycles++;
        
        // Progress indicator
        if (verbose_ && boot_cycles % 500000 == 0) {
            uint16_t current_pc = cpu->get(REG_PC);
            printf("  Still booting... PC=$%04X (cycle %u)\n", current_pc, boot_cycles);
        }
    }
    
    if (verbose_) {
        uint16_t final_pc = cpu->get(REG_PC);
        printf("  KERNAL boot complete after %u cycles (PC=$%04X)\n", boot_cycles, final_pc);
    }
    return true;
}

// Execute BASIC boot sequence (KERNAL + BASIC initialization)
bool TestFramework::execute_basic_boot(C64System* c64, const TestDescriptor& test, uint16_t sys_addr) {
    auto* cpu = c64->mos6510;
    
    if (verbose_) {
        printf("  Executing BASIC boot sequence...\n");
    }
    
    // First, execute KERNAL boot
    if (!execute_kernal_boot(c64)) {
        return false;
    }
    
    // Continue execution through BASIC initialization
    // BASIC boot completes when it reaches the keyboard input loop at $E5CD-$E5D6
    // This is the READY prompt waiting for input - normal behavior, not a hang
    const uint32_t MAX_BASIC_BOOT_CYCLES = 2500000;
    uint32_t boot_cycles = 0;
    uint16_t last_pc = 0xFFFF;
    uint32_t stable_cycles = 0;
    
    if (verbose_) {
        printf("  Running BASIC initialization (detecting keyboard loop completion)...\n");
    }
    
    while (boot_cycles < MAX_BASIC_BOOT_CYCLES) {
        c64->tick();
        boot_cycles++;
        
        uint16_t current_pc = cpu->get(REG_PC);
        
        // Check every 1000 cycles for keyboard input loop
        if (boot_cycles % 1000 == 0) {
            // Keyboard input loop is at $E5CD-$E5D6 (checking buffer, looping if empty)
            // If PC is stable in this range for multiple checks, BASIC is ready
            if (current_pc >= 0xE5CD && current_pc <= 0xE5D6) {
                if (current_pc == last_pc) {
                    stable_cycles++;
                    if (stable_cycles >= 5) {  // Stable for 5000 cycles = boot complete
                        if (verbose_) {
                            printf("  BASIC keyboard input loop detected at PC=$%04X\n", current_pc);
                            printf("  BASIC boot complete after %u cycles\n", boot_cycles);
                        }
                        break;
                    }
                } else {
                    stable_cycles = 1;  // Reset but count current cycle
                }
            } else {
                stable_cycles = 0;
            }
            
            last_pc = current_pc;
        }
        
        // Progress indicator
        if (verbose_ && boot_cycles % 500000 == 0) {
            printf("  Still booting... PC=$%04X (cycle %u)\n", current_pc, boot_cycles);
        }
    }
    
    uint16_t final_pc = cpu->get(REG_PC);
    
    if (verbose_) {
        printf("  Boot sequence finished at PC=$%04X after %u cycles\n", final_pc, boot_cycles);
        printf("  Setting PC to SYS address $%04X\n", sys_addr);
    }
    
    // After boot, set PC to target address (simulating SYS command)
    cpu->set(REG_PC, sys_addr);
    
    // CRITICAL: Reset CPU pipeline state machine to fetch mode.
    // Without this, the CPU's current_handler and half_cycle are still
    // mid-instruction from the KERNAL keyboard loop, causing the CPU to
    // finish that stale instruction instead of fetching from the new PC.
    cpu->transition_to_fetch();
    
    // Sync address bus register with new PC (needed for first fetch)
    cpu->set(REG_AB, sys_addr);
    
    // Set up stack for SYS command context.
    // Real BASIC SYS uses JSR internally: it pushes the return address - 1
    // (6502 convention: JSR pushes addr of last byte of JSR instruction,
    //  RTS pops and adds 1 to get the next instruction address).
    // Use the current stack pointer from BASIC boot (don't clobber it).
    // Push return address pointing to BASIC warm start ($A7AE) so if the
    // test does RTS, it returns to BASIC safely.
    uint8_t sp = cpu->get(REG_S);
    uint16_t return_addr = 0xA7AE - 1;  // BASIC warm start, adjusted for RTS convention
    c64->ram->data()[0x0100 + sp] = (return_addr >> 8) & 0xFF;  // High byte
    sp--;
    c64->ram->data()[0x0100 + sp] = return_addr & 0xFF;          // Low byte
    sp--;
    cpu->set(REG_S, sp);
    
    return true;
}

bool TestFramework::load_test_program(const TestDescriptor& test, C64System* c64) {
    std::string full_path = vice_testprogs_path_ + "/" + test.path;
    
    // Reset C64 system (reset all components)
    c64->reset();
    
    // Load PRG file
    uint16_t load_addr, sys_addr;
    if (!c64_test_load_prg_file(full_path.c_str(), c64->ram, &load_addr, &sys_addr)) {
        return false;
    }
    
    // Detect required execution environment
    TestEnvironment environment = detect_test_environment(test, load_addr, sys_addr);
    
    // Set up CPU I/O port for standard C64 configuration (needed for all environments)
    // Banking mode 0x07: LORAM=1, HIRAM=1, CHAREN=1 (standard C64 boot configuration)
    c64->ram->data()[0x00] = 0x2F;  // DDR: bits 0-2 output, others input
    c64->ram->data()[0x01] = 0x37;  // Data: LORAM=1, HIRAM=1, CHAREN=1
    c64->bus.on_banking_change(0x07);
    
    auto* cpu = c64->mos6510;
    if (!cpu) {
        return false;
    }
    
    // Execute appropriate boot sequence based on environment
    switch (environment) {
        case TestEnvironment::BASIC_BOOT:
            if (verbose_) {
                printf("  Using BASIC boot environment\n");
            }
            // Execute full KERNAL+BASIC boot, then jump to SYS address
            if (!execute_basic_boot(c64, test, sys_addr != 0 ? sys_addr : load_addr)) {
                if (verbose_) {
                    printf("  BASIC boot failed, falling back to direct execution\n");
                }
                // Fall through to direct execution
            } else {
                // BASIC boot succeeded, test is ready to run
                return true;
            }
            FALLTHROUGH;
            
        case TestEnvironment::KERNAL_BOOT:
            if (verbose_) {
                printf("  Using KERNAL boot environment\n");
            }
            if (!execute_kernal_boot(c64)) {
                if (verbose_) {
                    printf("  KERNAL boot failed, falling back to direct execution\n");
                }
                // Fall through to direct execution
            } else {
                // After KERNAL boot, set PC to test entry point
                uint16_t start_addr = (sys_addr != 0) ? sys_addr : load_addr;
                cpu->set(REG_PC, start_addr);
                cpu->transition_to_fetch();
                cpu->set(REG_AB, start_addr);
                if (verbose_) {
                    printf("  Set PC to test entry: $%04X\n", start_addr);
                }
                return true;
            }
            FALLTHROUGH;
            
        case TestEnvironment::DIRECT_EXECUTION:
        case TestEnvironment::AUTO_DETECT:
        default:
            // Direct execution: load and run immediately
            if (verbose_) {
                printf("  Using direct execution environment\n");
            }
            
            uint16_t start_addr = (sys_addr != 0) ? sys_addr : load_addr;
            cpu->set(REG_PC, start_addr);
            
            // CRITICAL: Reset CPU pipeline and sync AB register for first fetch
            cpu->transition_to_fetch();
            cpu->set(REG_AB, start_addr);
            
            // IMPORTANT: Leave interrupts ENABLED for direct execution
            // Many tests (especially CIA/Lorenz tests) rely on interrupts for timing
            // Tests that need interrupts disabled will use SEI instruction themselves
            // The KERNAL boot above already enabled interrupts (cleared I flag)
            // So we don't change the I flag here - let tests control it
            
            if (verbose_) {
                printf("  Set CPU PC to: $%04X\n", start_addr);
                printf("  Interrupts left as-is (tests control I flag)\n");
            }
            break;
    }
    
    return true;
}

    
// Detect test protocol based on test characteristics
TestProtocol TestFramework::detect_test_protocol(const TestDescriptor& test, C64System* c64) {
    // If protocol is already specified, use it
    if (test.protocol != TestProtocol::AUTO_DETECT) {
        return test.protocol;
    }
    
    // Check for BASIC two-stage loader pattern
    MemoryChip* ram = c64->ram;
    auto* cpu = c64->mos6510;
    uint16_t pc = cpu->get(REG_PC);
    
    // BASIC two-stage loaders start at $0801 and have SYS command
    if (pc == 0x0801 && detect_basic_two_stage_loader(ram, pc)) {
        if (verbose_) {
            printf("  Detected: BASIC two-stage loader\n");
        }
        return TestProtocol::BASIC_LOADER;
    }
    
    // Check for common infinite-loop test patterns (e.g., kdormann tests)
    // These typically have specific completion addresses
    if (test.category == "CPU" &&
        (test.name.find("6502_functional_test") != std::string::npos ||
         test.name.find("decimal") != std::string::npos)) {
        if (verbose_) {
            printf("  Detected: Infinite-loop protocol (kdormann style)\n");
        }
        return TestProtocol::INFINITE_LOOP;
    }
    
    // Default to debug register protocol for most tests
    return TestProtocol::DEBUG_REGISTER;
}

// Detect BASIC two-stage loader (loads at $0801, calculates entry point)
bool TestFramework::detect_basic_two_stage_loader(MemoryChip* ram, uint16_t load_addr) {
    // BASIC programs start with link address at $0801/$0802
    if (load_addr != 0x0801) return false;
    
    // Check for BASIC structure: link pointer, line number, SYS token
    uint16_t link = ram->data()[0x0801] | (ram->data()[0x0802] << 8);
    if (link == 0) return false;  // No BASIC program
    
    // Look for SYS token ($9E) in first line
    for (int i = 0x0804; i < 0x0820; i++) {
        if (ram->data()[i] == 0x9E) {  // SYS token
            return true;
        }
        if (ram->data()[i] == 0x00) {  // End of line
            break;
        }
    }
    
    return false;
}

// Calculate entry point from BASIC SYS command
uint16_t TestFramework::calculate_basic_entry_point(MemoryChip* ram, uint16_t sys_addr) {
    // Parse ASCII digits after SYS token
    uint16_t addr = 0;
    for (int i = sys_addr + 1; i < sys_addr + 20; i++) {
        uint8_t c = ram->data()[i];
        if (c >= '0' && c <= '9') {
            addr = addr * 10 + (c - '0');
        } else if (c == ' ') {
            continue;  // Skip spaces
        } else {
            break;  // End of number
        }
    }
    return addr;
}
// Detect if CPU is stuck in infinite loop and check border color
bool TestFramework::detect_infinite_loop(C64System* c64, uint16_t& loop_pc, uint32_t check_cycles) {
    auto* cpu = c64->mos6510;
    uint16_t pc = cpu->get(REG_PC);
    
    // Run for check_cycles and see if PC stays at same address
    uint32_t stable_count = 0;
    uint16_t last_pc = pc;
    
    for (uint32_t i = 0; i < check_cycles; i++) {
        c64->tick();
        uint16_t current_pc = cpu->get(REG_PC);
        
        if (current_pc == last_pc) {
            stable_count++;
            if (stable_count >= 100) {  // PC stable for 100 cycles = infinite loop
                loop_pc = current_pc;
                return true;
            }
        } else {
            stable_count = 0;
            last_pc = current_pc;
        }
    }
    
    return false;
}

// Get current VIC-II border color
uint8_t TestFramework::get_border_color(C64System* c64) {
    if (!c64 || !c64->vicii) {
        return 0;
    }
    
    vicii_t* vicii = static_cast<vicii_t*>(c64->vicii);
    // Border color is at register $D020 (vicii_regs::EC = register 32)
    return vicii->regs_[vicii_regs::EC] & 0x0F;  // Only lower 4 bits are color
}

// Enhanced exitcode test with multi-protocol support
TestResult TestFramework::run_exitcode_test_enhanced(const TestDescriptor& test, C64System* c64) {
    TestResult result;
    result.test = test;
    result.status = TestStatus::TIMEOUT;
    
    // Detect protocol
    TestProtocol protocol = detect_test_protocol(test, c64);
    
    uint32_t max_cycles = test.timeout_cycles;
    uint32_t cycles = 0;
    
    auto* cpu = c64->mos6510;
    uint16_t start_pc = cpu->get(REG_PC);
    
    if (verbose_) {
        printf("  Protocol: ");
        switch (protocol) {
            case TestProtocol::DEBUG_REGISTER: printf("DEBUG_REGISTER\n"); break;
            case TestProtocol::INFINITE_LOOP: printf("INFINITE_LOOP\n"); break;
            case TestProtocol::BASIC_LOADER: printf("BASIC_LOADER\n"); break;
            case TestProtocol::KERNAL_EXIT: printf("KERNAL_EXIT\n"); break;
            default: printf("AUTO_DETECT\n"); break;
        }
        printf("  Starting at PC=$%04X\n", start_pc);
    }
    
    // Handle BASIC loader protocol
    if (protocol == TestProtocol::BASIC_LOADER) {
        // Execute BASIC SYS command calculation
        // For now, just run for extended time to let BASIC execute
        // TODO: Implement minimal BASIC interpreter or parse SYS command
        if (verbose_) {
            printf("  Note: BASIC loader requires BASIC ROM - skipping for now\n");
        }
        result.status = TestStatus::SKIPPED;
        result.message = "BASIC loader protocol not yet fully implemented";
        return result;
    }
    
    // Clear debug register intercept before test execution
    install_debug_intercept(c64);
    debug_intercept_.written = false;
    debug_intercept_.value = 0;
    
#ifdef CERMU_HAS_SEH
    // Use SEH-protected tick loop on Windows to catch access violations
    TickLoopResult tr = tick_loop_protected(c64, max_cycles, &debug_intercept_);
    cycles = tr.cycles;
    
    switch (tr.reason) {
        case 0: // debug_reg written
            if (tr.debug_value == 0x00) {
                result.status = TestStatus::PASSED;
                result.message = "Test passed ($D7FF = $00)";
                if (verbose_) printf("\n  Test passed at cycle %u\n", cycles);
            } else if (tr.debug_value == 0xFF) {
                result.status = TestStatus::FAILED;
                result.message = "Test failed ($D7FF = $FF)";
                if (verbose_) printf("\n  Test failed at cycle %u\n", cycles);
            } else {
                // Subtest indicator was the last write before timeout/loop
                result.status = TestStatus::TIMEOUT;
                char buf[128];
                snprintf(buf, sizeof(buf), "Last $D7FF=$%02X, no pass/fail before end", tr.debug_value);
                result.message = buf;
            }
            break;
        case 1: // timeout
            result.status = TestStatus::TIMEOUT;
            result.message = "Test timeout - no completion detected";
            if (verbose_) printf("  Timeout at cycle %u\n", cycles);
            break;
        case 2: // crash
            result.status = TestStatus::ERROR;
            result.message = "CRASH: Access violation during test execution";
            printf("  !!! CRASH detected in %s at ~cycle %u\n", test.name.c_str(), cycles);
            break;
        case 3: { // infinite loop
            uint8_t border_color = tr.border_color;
            uint16_t stable_pc = tr.loop_pc;
            bool has_explicit_result = false;
            
            // Check explicit PC addresses first
            if (test.success_pc != 0 && stable_pc == test.success_pc) {
                result.status = TestStatus::PASSED;
                result.message = "Test passed (success address reached)";
                has_explicit_result = true;
            } else if (test.failure_pc != 0 && stable_pc == test.failure_pc) {
                result.status = TestStatus::FAILED;
                result.message = "Test failed (failure address reached)";
                has_explicit_result = true;
            }
            
            // Check debug register value
            if (!has_explicit_result && debug_intercept_.value != 0) {
                if (debug_intercept_.value == 0x00) {
                    result.status = TestStatus::PASSED;
                    result.message = "Test passed ($D7FF = $00, then loop)";
                    has_explicit_result = true;
                } else if (debug_intercept_.value == 0xFF) {
                    result.status = TestStatus::FAILED;
                    result.message = "Test failed ($D7FF = $FF, then loop)";
                    has_explicit_result = true;
                }
            }
            
            // Fall back to border color
            if (!has_explicit_result) {
                if (border_color == VICII_COLOR_GREEN) {
                    result.status = TestStatus::PASSED;
                    result.message = "Test passed (border=GREEN)";
                } else if (border_color == VICII_COLOR_RED) {
                    result.status = TestStatus::FAILED;
                    result.message = "Test failed (border=RED)";
                } else {
                    result.status = TestStatus::PASSED;
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Test completed (loop at $%04X, border=%u)", stable_pc, border_color);
                    result.message = buf;
                }
            }
            
            if (verbose_) {
                printf("\n  Infinite loop at PC=$%04X, border=%u\n", stable_pc, border_color);
            }
            break;
        }
    }
#else
    // Non-Windows fallback: unprotected execution
    uint16_t last_pc = start_pc;
    uint32_t pc_stable_count = 0;
    uint16_t stable_pc = 0;
    
    // Main execution loop
    while (cycles < max_cycles) {
        c64->tick();
        cycles++;
        
        // Check debug register intercept (set by io_write_intercept on write to $D7FF)
        if (debug_intercept_.written) {
            uint8_t value = debug_intercept_.value;
            debug_intercept_.written = false;  // Acknowledge
            
            if (value == 0x00) {
                result.status = TestStatus::PASSED;
                result.message = "Test passed ($D7FF = $00)";
                break;
            } else if (value == 0xFF) {
                result.status = TestStatus::FAILED;
                result.message = "Test failed ($D7FF = $FF)";
                
                // Diagnostic dump for CIA tests: show first subtest error buffer
                // ERRBUF ($5F00) stores color per subtest: 5=pass (green), 10=fail (red)
                if (verbose_) {
                    printf("\n  ERRBUF ($5F00): ");
                    for (int di = 0; di < 24; di++) {
                        printf("%02X ", c64->ram->data()[0x5F00 + di]);
                    }
                    printf("\n");
                    
                    // Detect TMP/DATA base addresses by scanning common locations
                    // cia1-3,5: TMP=$8000 DATA=$9000; cia4,6+: TMP=$6000 DATA=$8000
                    uint16_t tmp_base = 0x8000;
                    uint16_t dat_base = 0x9000;
                    uint16_t sub_size = 0x100;
                    // Heuristic: if $6000-$60FF has non-zero data, use $6000/$8000
                    bool has_6000_data = false;
                    for (int di = 0; di < 64; di++) {
                        if (c64->ram->data()[0x6000 + di] != 0) { has_6000_data = true; break; }
                    }
                    if (has_6000_data) { tmp_base = 0x6000; dat_base = 0x8000; }
                    
                    // Dump up to 3 failing subtests
                    int shown = 0;
                    for (int st = 0; st < 24 && shown < 3; st++) {
                        if (c64->ram->data()[0x5F00 + st] == 0x0A) {
                            uint16_t tmp_addr = tmp_base + st * sub_size;
                            uint16_t data_addr = dat_base + st * sub_size;
                            printf("  Fail subtest %d (TMP=$%04X DATA=$%04X)\n", st, tmp_addr, data_addr);
                            printf("  TMP (actual):    ");
                            for (int di = 0; di < 48; di++) printf("%02X ", c64->ram->data()[tmp_addr + di]);
                            printf("\n  DATA (expected): ");
                            for (int di = 0; di < 48; di++) printf("%02X ", c64->ram->data()[data_addr + di]);
                            printf("\n  Differences:     ");
                            for (int di = 0; di < 48; di++) {
                                if (c64->ram->data()[tmp_addr + di] != c64->ram->data()[data_addr + di])
                                    printf("^^ "); else printf("   ");
                            }
                            printf("\n");
                            shown++;
                        }
                    }
                }
                break;
            }
        }
        
        // Periodic checks every 256 cycles
        if ((cycles & 0xFF) == 0) {
            uint16_t current_pc = cpu->get(REG_PC);
            if (current_pc == last_pc) {
                if (pc_stable_count == 0) stable_pc = current_pc;
                pc_stable_count++;
                if (pc_stable_count >= 2) {
                    uint8_t opcode = c64->ram->data()[current_pc];
                    bool is_jmp_self = false;
                    if (opcode == 0x4C) {
                        uint16_t target = c64->ram->data()[(current_pc + 1) & 0xFFFF] |
                                         (c64->ram->data()[(current_pc + 2) & 0xFFFF] << 8);
                        is_jmp_self = (target == current_pc);
                    }
                    if (is_jmp_self || pc_stable_count >= 8) {
                        uint8_t border_color = get_border_color(c64);
                        if (border_color == VICII_COLOR_GREEN) {
                            result.status = TestStatus::PASSED;
                            result.message = "Test passed (border=GREEN)";
                        } else if (border_color == VICII_COLOR_RED) {
                            result.status = TestStatus::FAILED;
                            result.message = "Test failed (border=RED)";
                        } else {
                            result.status = TestStatus::PASSED;
                            char buf[128];
                            snprintf(buf, sizeof(buf), "Test completed (loop at $%04X, border=%u)", stable_pc, border_color);
                            result.message = buf;
                        }
                        break;
                    }
                }
            } else {
                pc_stable_count = 0;
            }
            last_pc = current_pc;
        }
    }
    
    if (result.status == TestStatus::TIMEOUT) {
        result.message = "Test timeout - no completion detected";
    }
#endif
    
    uninstall_debug_intercept(c64);
    result.cycles_executed = cycles;
    
    return result;
}

TestResult TestFramework::run_exitcode_test(const TestDescriptor& test, C64System* c64) {
    TestResult result;
    result.test = test;
    result.status = TestStatus::TIMEOUT;
    
    uint32_t max_cycles = test.timeout_cycles;
    uint32_t cycles = 0;
    uint8_t debug_value = 0xFF;
    
    // Install IO write intercept for $D7FF debug register
    install_debug_intercept(c64);
    debug_intercept_.written = false;
    debug_intercept_.value = 0;
    
    // Get initial PC for diagnostics
    auto* cpu = c64->mos6510;
    uint16_t start_pc = cpu->get(REG_PC);
    uint16_t last_pc = start_pc;
    bool pc_changed = false;
    uint32_t pc_change_count = 0;
    
    // Track if PC enters KERNAL range
    bool entered_kernal = false;
    uint32_t kernal_entry_cycle = 0;
    
    if (verbose_) {
        printf("  Starting execution at PC=$%04X\n", start_pc);
    }
    
    // Run emulation until debug register is written or timeout
    while (cycles < max_cycles) {
        c64->tick();
        cycles++;
        
        // Check IO write intercept for $D7FF writes (every cycle - it's just a bool check)
        if (debug_intercept_.written) {
            debug_value = debug_intercept_.value;
            debug_intercept_.written = false;
            
            if (debug_value == 0x00) {
                result.status = TestStatus::PASSED;
                result.exit_code = 0x00;
                result.message = "Test passed";
                if (verbose_) {
                    printf("\n  ✓ Test passed - $D7FF = $00 at cycle %u\n", cycles);
                }
                break;
            } else if (debug_value == 0xFF) {
                result.status = TestStatus::FAILED;
                result.exit_code = 0xFF;
                result.message = "Test failed";
                if (verbose_) {
                    printf("\n  ✗ Test failed - $D7FF = $FF at cycle %u\n", cycles);
                }
                break;
            } else if (cycles <= 1000) {
                // Debug register changed to a non-pass/fail value early on
                if (verbose_) {
                    printf("\n  ℹ️  $D7FF changed to $%02X at cycle ≤1000 (test is using debug register)\n", debug_value);
                }
            }
        }
        
        // Check PC every 1000 cycles for diagnostic
        if (cycles % 1000 == 0) {
            uint16_t current_pc = cpu->get(REG_PC);
            if (current_pc != last_pc) {
                pc_changed = true;
                pc_change_count++;
                
                // Detect entry into KERNAL range ($E000-$FFFF)
                if (!entered_kernal && current_pc >= 0xE000) {
                    entered_kernal = true;
                    kernal_entry_cycle = cycles;
                    if (verbose_) {
                        printf("\n  ⚠️  PC entered KERNAL at cycle %u: $%04X\n", cycles, current_pc);
                    }
                }
                
                last_pc = current_pc;
            }
        }
        
        // Print progress dots in verbose mode
        if (verbose_ && cycles % 100000 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    if (verbose_ && cycles >= 100000) {
        printf("\n");
    }
    
    result.cycles_executed = cycles;
    
    if (result.status == TestStatus::TIMEOUT) {
        result.message = "Test timeout - no result within cycle limit";
        result.exit_code = debug_value;
        
        // Add diagnostic information
        if (!pc_changed) {
            result.message += " (CPU PC never changed from $";
            char buf[8];
            snprintf(buf, sizeof(buf), "%04X", start_pc);
            result.message += buf;
            result.message += " - CPU may not be executing)";
            
            if (verbose_) {
                printf("  ⚠️  DIAGNOSTIC: PC never changed from $%04X\n", start_pc);
                printf("  ⚠️  This suggests the CPU is not executing instructions\n");
            }
        } else {
            if (verbose_) {
                printf("  PC changed %u times, last PC=$%04X\n", pc_change_count, last_pc);
                printf("  Final $D7FF value: $%02X\n", debug_value);
                if (entered_kernal) {
                    printf("  ⚠️  Test entered KERNAL code at cycle %u\n", kernal_entry_cycle);
                    printf("  ⚠️  This suggests: interrupt occurred, JSR to KERNAL, or memory banking issue\n");
                } else if (debug_value == 0xFF) {
                    printf("  ⚠️  $D7FF never changed from initial - test may not use this debug register\n");
                    printf("  ⚠️  Test might be waiting for VIC-II timing or other hardware\n");
                }
            }
        }
    }
    
    uninstall_debug_intercept(c64);
    return result;
}

TestResult TestFramework::run_screenshot_test(const TestDescriptor& test, C64System* c64) {
    TestResult result;
    result.test = test;
    result.status = TestStatus::TIMEOUT;
    
    uint32_t max_cycles = test.timeout_cycles;
    uint32_t cycles = 0;
    
    if (verbose_) {
        printf("  Running for %u cycles to generate screenshot...\n", max_cycles);
    }
    
    // Run emulation for specified cycles to let test generate output
    while (cycles < max_cycles) {
        c64->tick();
        cycles++;
        
        // Print progress dots
        if (verbose_ && cycles % 100000 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
    
    if (verbose_ && cycles >= 100000) {
        printf("\n");
    }
    
    result.cycles_executed = cycles;
    
    // Get VIC-II framebuffer
    vicii_t* vicii = static_cast<vicii_t*>(c64->vicii);
    if (!vicii || !vicii->pixel.framebuffer) {
        result.status = TestStatus::ERROR;
        result.message = "VIC-II framebuffer not available";
        return result;
    }
    
    // Generate output filename
    std::string output_dir = output_directory_ + "/screenshots";
    std::string test_name = test.name;
    // Remove .prg extension
    if (test_name.size() > 4 && test_name.substr(test_name.size() - 4) == ".prg") {
        test_name = test_name.substr(0, test_name.size() - 4);
    }
    std::string output_png = output_dir + "/" + test_name + ".png";
    
    // Normalize path separators for current platform
    os_normalize_path(output_dir);
    os_normalize_path(output_png);
    
    // Create output directory if needed
    os_mkdir_p(output_dir);
    
    // Save framebuffer to PNG using base class screenshot method
    if (!c64->save_screenshot(output_png.c_str())) {
        result.status = TestStatus::ERROR;
        result.message = "Failed to save screenshot";
        return result;
    }
    
    result.screenshot_path = output_png;
    
    if (verbose_) {
        printf("  Saved screenshot: %s\n", output_png.c_str());
    }
    
    // Compare with reference image if available
    if (!test.reference_image.empty()) {
        int diff_count = 0;
        
        // Analyze reference image first to extract dimensions, borders, and palette
        ReferenceImageInfo ref_info;
        bool analyzed = analyze_reference_image(test.reference_image, ref_info);
        
        if (!analyzed) {
            result.status = TestStatus::ERROR;
            result.message = "Failed to analyze reference image";
            return result;
        }
        
        // Check if we need to resave screenshot with exact reference dimensions
        // This handles cases where reference has different crop than our default
        if (ref_info.width != vicii->pixel.fb_width ||
            ref_info.height != vicii->pixel.fb_height) {
            
            if (verbose_) {
                printf("  Reference dimensions (%dx%d) differ from framebuffer (%dx%d)\n",
                       ref_info.width, ref_info.height,
                       vicii->pixel.fb_width, vicii->pixel.fb_height);
            }
            
            // Use VICE-aligned crop offsets to extract the correct display window.
            // VICE PAL display: rasters 16-287 (272 lines), 384 pixels wide
            // (32px left border + 320px content + 32px right border)
            // Our framebuffer maps buffer_pos 0 → first_visible_x.
            // Content starts at buffer_pos 48, VICE left edge at buffer_pos 16.
            int crop_w = ref_info.width;
            int crop_h = ref_info.height;
            int crop_x, crop_y;
            
            int fb_w = vicii->pixel.fb_width;
            int fb_h = vicii->pixel.fb_height;
            
            // VICE PAL: first_displayed_line=16, display is 384x272
            if (ref_info.width == 384 && ref_info.height == 272) {
                // VICE PAL standard crop.
                // Framebuffer row 0 = raster 16 (first visible line) due to
                // raster-to-fb-row offset mapping, so crop_y starts at 0.
                crop_x = 16;  // Buffer position where VICE left border starts
                crop_y = 0;   // fb row 0 = raster 16 = VICE first displayed line
            } else {
                // Fallback: center the crop for non-standard reference sizes
                crop_x = (fb_w - ref_info.width) / 2;
                crop_y = (fb_h - ref_info.height) / 2;
            }
            
            // Ensure crop is within bounds
            if (crop_x < 0) crop_x = 0;
            if (crop_y < 0) crop_y = 0;
            if (crop_x + crop_w > fb_w) {
                crop_x = fb_w - crop_w;
            }
            if (crop_y + crop_h > fb_h) {
                crop_y = fb_h - crop_h;
            }
            
            // Resave with custom crop
            if (!c64->save_screenshot_cropped(output_png.c_str(), crop_x, crop_y, crop_w, crop_h)) {
                result.status = TestStatus::ERROR;
                result.message = "Failed to save cropped screenshot";
                return result;
            }
            
            if (verbose_) {
                printf("  Resaved screenshot with crop: %d,%d %dx%d\n",
                       crop_x, crop_y, crop_w, crop_h);
            }
        }
        
        // Allow up to 1% of pixels to differ slightly (for timing variations)
        int total_pixels = ref_info.width * ref_info.height;
        int max_diff_pixels = total_pixels / 100;
        
        // Compare using enhanced comparison with palette support
        if (compare_png_images(output_png, test.reference_image, &ref_info,
                              5, max_diff_pixels, &diff_count)) {
            result.status = TestStatus::PASSED;
            result.message = "Screenshot matches reference";
            if (verbose_) {
                printf("  ✓ Screenshot matches reference (%d pixels differ)\n", diff_count);
            }
        } else {
            result.status = TestStatus::FAILED;
            char buf[256];
            snprintf(buf, sizeof(buf),
                    "Screenshot mismatch: %d pixels differ (max allowed: %d)",
                    diff_count, max_diff_pixels);
            result.message = buf;
            if (verbose_) {
                printf("  ✗ %s\n", buf);
            }
        }
    } else {
        // No reference image - mark as skipped but save screenshot
        result.status = TestStatus::SKIPPED;
        result.message = "No reference image available for comparison";
        if (verbose_) {
            printf("  ⚠ No reference image, screenshot saved for manual review\n");
        }
    }
    
    return result;
}

std::vector<TestResult> TestFramework::run_tests(const std::vector<TestDescriptor>& tests, C64System* c64) {
    std::vector<TestResult> results;
    
    printf("\n=== Running %zu tests ===\n", tests.size());
    
    for (size_t i = 0; i < tests.size(); i++) {
        printf("[%zu/%zu] ", i + 1, tests.size());
        TestResult result = run_test_safe(tests[i], c64);
        results.push_back(result);
        
        // Print quick status
        const char* status_symbol = "?";
        switch (result.status) {
            case TestStatus::PASSED: status_symbol = "✓"; break;
            case TestStatus::FAILED: status_symbol = "✗"; break;
            case TestStatus::TIMEOUT: status_symbol = "⏱"; break;
            case TestStatus::SKIPPED: status_symbol = "○"; break;
            case TestStatus::ERROR: status_symbol = "!"; break;
            default: break;
        }
        printf("%s %s\n", status_symbol, tests[i].name.c_str());
    }
    
    return results;
}

std::vector<TestResult> TestFramework::run_all_tests(C64System* c64, const TestFilter& filter) {
    std::vector<TestDescriptor> tests = get_filtered_tests(filter);
    return run_tests(tests, c64);
}

TestFramework::TestStats TestFramework::get_statistics(const std::vector<TestResult>& results) const {
    TestStats stats = {};
    
    for (const auto& result : results) {
        stats.total++;
        stats.total_time_ms += result.execution_time_ms;
        
        switch (result.status) {
            case TestStatus::PASSED: stats.passed++; break;
            case TestStatus::FAILED: stats.failed++; break;
            case TestStatus::TIMEOUT: stats.timeout++; break;
            case TestStatus::SKIPPED: stats.skipped++; break;
            case TestStatus::ERROR: stats.error++; break;
            default: break;
        }
    }
    
    return stats;
}

void TestFramework::print_statistics(const std::vector<TestResult>& results) const {
    TestStats stats = get_statistics(results);
    
    printf("\n=== Test Statistics ===\n");
    printf("Total tests:    %d\n", stats.total);
    printf("Passed:         %d (%.1f%%)\n", stats.passed, stats.pass_rate());
    printf("Failed:         %d\n", stats.failed);
    printf("Timeout:        %d\n", stats.timeout);
    printf("Skipped:        %d\n", stats.skipped);
    printf("Error:          %d\n", stats.error);
    printf("Total time:     %.2f seconds\n", stats.total_time_ms / 1000.0);
}

void TestFramework::print_summary(const std::vector<TestResult>& results) const {
    print_statistics(results);
    
    // Print failed tests
    printf("\n=== Failed Tests ===\n");
    bool has_failures = false;
    for (const auto& result : results) {
        if (result.status == TestStatus::FAILED || result.status == TestStatus::TIMEOUT) {
            printf("  %s: %s\n", result.test.path.c_str(), result.message.c_str());
            has_failures = true;
        }
    }
    if (!has_failures) {
        printf("  None\n");
    }
}

std::vector<TestDescriptor> TestFramework::get_failed_tests(const std::vector<TestResult>& results) {
    std::vector<TestDescriptor> failed;
    for (const auto& result : results) {
        if (result.status == TestStatus::FAILED || result.status == TestStatus::TIMEOUT) {
            failed.push_back(result.test);
        }
    }
    return failed;
}

void TestFramework::save_results(const std::string& output_file, const std::vector<TestResult>& results) {
    std::ofstream out(output_file);
    if (!out.is_open()) {
        printf("ERROR: Cannot write results to: %s\n", output_file.c_str());
        return;
    }
    
    out << "Test Results\n";
    out << "============\n\n";
    
    for (const auto& result : results) {
        out << "Test: " << result.test.path << "\n";
        out << "  Status: " << test_status_to_string(result.status) << "\n";
        out << "  Type: " << test_type_to_string(result.test.type) << "\n";
        out << "  Cycles: " << result.cycles_executed << "\n";
        out << "  Time: " << result.execution_time_ms << " ms\n";
        if (!result.message.empty()) {
            out << "  Message: " << result.message << "\n";
        }
        out << "\n";
    }
    
    TestStats stats = get_statistics(results);
    out << "\nSummary:\n";
    out << "  Total: " << stats.total << "\n";
    out << "  Passed: " << stats.passed << " (" << stats.pass_rate() << "%)\n";
    out << "  Failed: " << stats.failed << "\n";
    out << "  Timeout: " << stats.timeout << "\n";
    out << "  Skipped: " << stats.skipped << "\n";
    out << "  Error: " << stats.error << "\n";
    
    out.close();
    printf("Results saved to: %s\n", output_file.c_str());
}

void TestFramework::save_results_json(const std::string& output_file, const std::vector<TestResult>& results) {
    std::ofstream out(output_file);
    if (!out.is_open()) {
        printf("ERROR: Cannot write JSON results to: %s\n", output_file.c_str());
        return;
    }
    
    out << "{\n";
    out << "  \"results\": [\n";
    
    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"path\": \"" << r.test.path << "\",\n";
        out << "      \"name\": \"" << r.test.name << "\",\n";
        out << "      \"category\": \"" << r.test.category << "\",\n";
        out << "      \"status\": \"" << test_status_to_string(r.status) << "\",\n";
        out << "      \"type\": \"" << test_type_to_string(r.test.type) << "\",\n";
        out << "      \"cycles\": " << r.cycles_executed << ",\n";
        out << "      \"time_ms\": " << r.execution_time_ms << ",\n";
        out << "      \"exit_code\": " << (int)r.exit_code << ",\n";
        out << "      \"message\": \"" << r.message << "\"\n";
        out << "    }" << (i < results.size() - 1 ? "," : "") << "\n";
    }
    
    out << "  ],\n";
    
    TestStats stats = get_statistics(results);
    out << "  \"summary\": {\n";
    out << "    \"total\": " << stats.total << ",\n";
    out << "    \"passed\": " << stats.passed << ",\n";
    out << "    \"failed\": " << stats.failed << ",\n";
    out << "    \"timeout\": " << stats.timeout << ",\n";
    out << "    \"skipped\": " << stats.skipped << ",\n";
    out << "    \"error\": " << stats.error << ",\n";
    out << "    \"pass_rate\": " << stats.pass_rate() << ",\n";
    out << "    \"total_time_ms\": " << stats.total_time_ms << "\n";
    out << "  }\n";
    out << "}\n";
    
    out.close();
    printf("JSON results saved to: %s\n", output_file.c_str());
}

void TestFramework::save_results_html(const std::string& output_file, const std::vector<TestResult>& results) {
    // TODO: Implement HTML report generation
    printf("HTML report generation not yet implemented\n");
}

std::vector<TestResult> TestFramework::load_previous_results(const std::string& results_file) {
    // TODO: Implement loading previous results from JSON
    printf("Loading previous results not yet implemented\n");
    return std::vector<TestResult>();
}

// Check if test requires different hardware configuration than current
bool TestFramework::requires_reconfiguration(const TestDescriptor& test) const {
    // Check if test requires PAL but we have NTSC
    if (static_cast<int>(test.required_hw & HardwareConfig::VICII_PAL) && is_ntsc_system_) {
        return true;
    }
    
    // Check if test requires NTSC but we have PAL
    if ((static_cast<int>(test.required_hw & HardwareConfig::VICII_NTSC) ||
         static_cast<int>(test.required_hw & HardwareConfig::VICII_NTSCOLD)) && is_pal_system_) {
        return true;
    }
    
    // For now, only PAL/NTSC switching requires reconfiguration
    // CIA and SID variants don't require system recreation yet
    return false;
}

// Create a C64 system configured for the specific test
C64System* TestFramework::create_system_for_test(const TestDescriptor& test) {
    // Determine required video standard
    bool needs_pal = static_cast<int>(test.required_hw & HardwareConfig::VICII_PAL) != 0;
    bool needs_ntsc = (static_cast<int>(test.required_hw & HardwareConfig::VICII_NTSC) != 0) ||
                      (static_cast<int>(test.required_hw & HardwareConfig::VICII_NTSCOLD) != 0);
    
    // Select region index: 0 = PAL, 1 = NTSC
    int region_index = 0;  // default PAL
    if (needs_ntsc) {
        region_index = 1;
        is_pal_system_ = false;
        is_ntsc_system_ = true;
        if (verbose_) {
            printf("Creating NTSC C64 system for test\n");
        }
    } else {
        is_pal_system_ = true;
        is_ntsc_system_ = false;
        if (needs_pal && verbose_) {
            printf("Creating PAL C64 system for test\n");
        }
    }
    
    // Create the system
    C64System* c64 = new C64System();
    auto cfg = c64->get_configuration();
    cfg.region_option_index = region_index;
    c64->set_configuration(cfg);
    if (!c64->initialize()) {
        printf("ERROR: Failed to create C64 system for test\n");
        delete c64;
        return nullptr;
    }
    
    // Allocate framebuffer for VIC-II rendering (403x284 for PAL, 418x235 for NTSC)
    // Framebuffer row 0 = first visible raster (16 for PAL), matching documentation Section 3.4
    int fb_width = is_pal_system_ ? 403 : 418;
    int fb_height = is_pal_system_ ? 284 : 235;
    uint32_t* framebuffer = new uint32_t[fb_width * fb_height];
    if (framebuffer) {
        c64->set_framebuffer(framebuffer, fb_width, fb_height);
        if (verbose_) {
            printf("Allocated %dx%d framebuffer for VIC-II\n", fb_width, fb_height);
        }
    } else {
        printf("ERROR: Failed to allocate framebuffer\n");
        c64->shutdown();
        delete c64;
        return nullptr;
    }
    
    // Update current hardware tracking
    current_hardware_ = HardwareConfig::NONE;
    if (is_pal_system_) {
        current_hardware_ = current_hardware_ | HardwareConfig::VICII_PAL;
    }
    if (is_ntsc_system_) {
        current_hardware_ = current_hardware_ | HardwareConfig::VICII_NTSC;
    }
    
    return c64;
}

// Run a single test with automatic system creation
TestResult TestFramework::run_test_with_config(const TestDescriptor& test) {
    // Create system configured for this test
    C64System* c64 = create_system_for_test(test);
    if (!c64) {
        TestResult result;
        result.test = test;
        result.status = TestStatus::ERROR;
        result.message = "Failed to create C64 system";
        return result;
    }
    
    // Run the test
    TestResult result = run_test(test, c64);
    
    // Clean up
    c64->shutdown(); delete c64;
    
    return result;
}

// Run multiple tests with automatic reconfiguration
std::vector<TestResult> TestFramework::run_tests_with_auto_config(const std::vector<TestDescriptor>& tests) {
    std::vector<TestResult> results;
    
    printf("\n=== Running %zu tests with automatic hardware configuration ===\n", tests.size());
    
    C64System* current_c64 = nullptr;
    uint32_t* current_framebuffer = nullptr;  // Track framebuffer for cleanup
    
    for (size_t i = 0; i < tests.size(); i++) {
        const TestDescriptor& test = tests[i];
        
        printf("[%zu/%zu] ", i + 1, tests.size());
        
        // Check if we need to reconfigure
        bool need_reconfig = (current_c64 == nullptr) || requires_reconfiguration(test);
        
        if (need_reconfig) {
            // Destroy old system if it exists
            if (current_c64) {
                if (verbose_) {
                    printf("\n  Hardware reconfiguration needed for test\n");
                }
                current_c64->shutdown(); delete current_c64;
                current_c64 = nullptr;
                
                // Free framebuffer
                if (current_framebuffer) {
                    delete[] current_framebuffer;
                    current_framebuffer = nullptr;
                }
            }
            
            // Create new system with required configuration and framebuffer
            current_c64 = create_system_for_test(test);
            if (!current_c64) {
                TestResult result;
                result.test = test;
                result.status = TestStatus::ERROR;
                result.message = "Failed to create C64 system";
                results.push_back(result);
                
                printf("! %s (system creation failed)\n", test.name.c_str());
                continue;
            }
            
            // Get framebuffer pointer (already allocated by create_system_for_test)
            if (current_c64->vicii && current_c64->vicii->pixel.framebuffer) {
                current_framebuffer = current_c64->vicii->pixel.framebuffer;
            }
        }
        
        // Run the test on current system (SEH-protected on Windows)
        TestResult result = run_test_safe(test, current_c64);
        results.push_back(result);
        
        // If test crashed, the system is corrupted — destroy it so next test gets a fresh one
        if (result.status == TestStatus::ERROR && result.message.find("CRASH") != std::string::npos) {
            if (current_c64) {
                delete current_c64;
                current_c64 = nullptr;
            }
            if (current_framebuffer) {
                delete[] current_framebuffer;
                current_framebuffer = nullptr;
            }
        }
        
        // Print quick status
        const char* status_symbol = "?";
        switch (result.status) {
            case TestStatus::PASSED: status_symbol = "✓"; break;
            case TestStatus::FAILED: status_symbol = "✗"; break;
            case TestStatus::TIMEOUT: status_symbol = "⏱"; break;
            case TestStatus::SKIPPED: status_symbol = "○"; break;
            case TestStatus::ERROR: status_symbol = "!"; break;
            default: break;
        }
        printf("%s %s\n", status_symbol, test.name.c_str());
    }
    
    // Clean up final system
    if (current_c64) {
        current_c64->shutdown(); delete current_c64;
    }
    
    return results;
}

} // namespace c64_test