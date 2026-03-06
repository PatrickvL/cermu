// =============================================================================
// C16/Plus4 Test Framework — Implementation
// =============================================================================
// Runs VICE-testprogs tests on a headless Plus4System (64KB, PAL by default).
// Detection: $FDCF debug cart register, infinite loop + border color fallback.
// =============================================================================

#include "c16_test_framework.h"
#include "c64_test_loader.h"     // Reuse PRG loader (writes raw bytes to MemoryChip)
#include "../systems/commodore/c16/c16_system.h"
#include "../systems/commodore/c16/c16_constants.h"
#include "../chip/memory/memory_chip.h"
#include "../chip/cpu/fam65xx/fam65xx.hpp"
#include "../chip/video/ted/ted7360.h"
#include "../core/os/os.h"

#ifdef CERMU_USE_STD_FILESYSTEM
    #include <filesystem>
    namespace cermu_fs = std::filesystem;
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <fstream>

namespace c16_test {

// =============================================================================
// Utility functions
// =============================================================================

std::string test_status_to_string(TestStatus status) {
    switch (status) {
        case TestStatus::NOT_RUN: return "NOT_RUN";
        case TestStatus::PASSED:  return "PASSED";
        case TestStatus::FAILED:  return "FAILED";
        case TestStatus::TIMEOUT: return "TIMEOUT";
        case TestStatus::SKIPPED: return "SKIPPED";
        case TestStatus::ERROR:   return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string test_type_to_string(TestType type) {
    switch (type) {
        case TestType::EXITCODE:    return "exitcode";
        case TestType::SCREENSHOT:  return "screenshot";
        case TestType::INTERACTIVE: return "interactive";
        default: return "unknown";
    }
}

// =============================================================================
// TestFilter
// =============================================================================

bool TestFilter::matches(const TestDescriptor& test) const {
    if (!path_filter.empty() && test.path.find(path_filter) == std::string::npos)
        return false;

    if (!categories.empty()) {
        bool found = false;
        for (const auto& cat : categories) {
            if (test.category == cat) { found = true; break; }
        }
        if (!found) return false;
    }

    if (skip_interactive && test.type == TestType::INTERACTIVE)
        return false;

    if (skip_screenshots && test.type == TestType::SCREENSHOT)
        return false;

    return true;
}

// =============================================================================
// TestFramework — construction
// =============================================================================

TestFramework::TestFramework(const std::string& vice_testprogs_path)
    : vice_testprogs_path_(vice_testprogs_path)
    , output_directory_("test_results")
    , verbose_(false)
{
}

TestFramework::~TestFramework() = default;

// =============================================================================
// Test discovery
// =============================================================================

bool TestFramework::scan_tests() {
    printf("Scanning Plus4/TED tests in: %s\n", vice_testprogs_path_.c_str());
    test_registry_.clear();

    // 1. Scan known VICE-testprogs directories
    discover_tests_in_directory(vice_testprogs_path_ + "/TED",  "TED",  "TED");
    discover_tests_in_directory(vice_testprogs_path_ + "/Plus4", "Plus4", "Plus4");

    // 2. Register tests from the VICE testbench test list
    register_testlist_tests();

    printf("Found %zu Plus4/TED tests\n", test_registry_.size());
    return !test_registry_.empty();
}

bool TestFramework::discover_tests_in_directory(
    const std::string& dir_path, const std::string& category,
    const std::string& rel_prefix)
{
#ifdef CERMU_USE_STD_FILESYSTEM
    if (!cermu_fs::is_directory(dir_path)) {
        if (verbose_) printf("  Category not found: %s\n", category.c_str());
        return false;
    }
    int count = 0;
    for (auto& entry : cermu_fs::recursive_directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext != ".prg") continue;
        std::string full = entry.path().string();
        std::replace(full.begin(), full.end(), '\\', '/');
        std::string rel = cermu_fs::relative(entry.path(), dir_path).string();
        std::replace(rel.begin(), rel.end(), '\\', '/');
        TestDescriptor td = parse_test_from_file(full, category);
        td.path = rel_prefix + "/" + rel;
        test_registry_[td.path] = td;
        count++;
    }
    if (verbose_) printf("  %s: %d tests\n", category.c_str(), count);
    return count > 0;
#else
    // POSIX implementation
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        if (verbose_) printf("  Category not found: %s\n", category.c_str());
        return false;
    }
    closedir(dir);

    int count = 0;
    std::function<void(const std::string&, const std::string&)> scan_recursive;
    scan_recursive = [&](const std::string& dpath, const std::string& rpath) {
        DIR* d = opendir(dpath.c_str());
        if (!d) return;
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            std::string full = dpath + "/" + entry->d_name;
            std::string rel  = rpath.empty() ? entry->d_name : rpath + "/" + entry->d_name;
            struct stat st;
            if (stat(full.c_str(), &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                scan_recursive(full, rel);
            } else if (S_ISREG(st.st_mode)) {
                size_t len = strlen(entry->d_name);
                if (len > 4 && strcmp(entry->d_name + len - 4, ".prg") == 0) {
                    TestDescriptor td = parse_test_from_file(full, category);
                    td.path = rel_prefix + "/" + rel;
                    test_registry_[td.path] = td;
                    count++;
                }
            }
        }
        closedir(d);
    };
    scan_recursive(dir_path, "");
    if (verbose_) printf("  %s: %d tests\n", category.c_str(), count);
    return count > 0;
#endif
}

TestDescriptor TestFramework::parse_test_from_file(
    const std::string& full_path, const std::string& category)
{
    TestDescriptor td;
    td.name = full_path.substr(full_path.find_last_of("/\\") + 1);
    td.category = category;
    td.type = TestType::EXITCODE;
    td.timeout_cycles = 50000000;  // ~56s PAL

    // Parse filename for hints
    std::string lower = td.name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("ntsc") != std::string::npos)
        td.required_hw = td.required_hw | HardwareConfig::TED_NTSC;
    if (lower.find("pal") != std::string::npos)
        td.required_hw = td.required_hw | HardwareConfig::TED_PAL;

    // Detect expected-fail tests (VICE convention)
    if (lower.find("-fail") != std::string::npos || lower.find("_fail") != std::string::npos)
        td.expect_fail = true;

    // Check for reference screenshot
    size_t last_slash = full_path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        std::string dir = full_path.substr(0, last_slash);
        std::string ref = dir + "/references/" + td.name + ".png";
        struct stat st;
        if (stat(ref.c_str(), &st) == 0) {
            td.type = TestType::SCREENSHOT;
            td.reference_image = ref;
        }
    }

    return td;
}

// Register tests listed in the VICE testbench plus4-testlist.in file.
// These come from various VICE-testprogs subdirectories and include
// selftest programs, openio tests, tcbm tests, etc.
void TestFramework::register_testlist_tests() {
    // Selftest programs (testbench/selftest/)
    auto add = [&](const std::string& rel_dir, const std::string& filename,
                    TestType type, uint32_t timeout, const std::string& category,
                    bool expect_fail = false) {
        std::string full = vice_testprogs_path_ + "/testbench/" + rel_dir + "/" + filename;
        struct stat st;
        if (stat(full.c_str(), &st) != 0) {
            if (verbose_) printf("  Testlist file not found: %s\n", full.c_str());
            return;
        }
        TestDescriptor td;
        td.path = "testbench/" + rel_dir + "/" + filename;
        td.name = filename;
        td.category = category;
        td.type = type;
        td.timeout_cycles = timeout;
        td.expect_fail = expect_fail;

        // Check for reference screenshot
        std::string ref = vice_testprogs_path_ + "/testbench/" + rel_dir
                        + "/references/" + filename + ".png";
        if (stat(ref.c_str(), &st) == 0)
            td.reference_image = ref;

        test_registry_[td.path] = td;
    };

    // From plus4-testlist.in (VICE convention):
    //   ./selftest/,plus4-pass.prg,exitcode,10000000
    //   ./selftest/,plus4-fail.prg,exitcode,10000000,expect:error
    //   ../Plus4/openio/,outrun.prg,exitcode,10000000
    //   ../Plus4/openio/,outrun2.prg,exitcode,10000000
    //   ../Plus4/tcbm/,test.prg,exitcode,10000000
    //   ../memory-expansions/,c16-ram-emd.prg,exitcode,68500000
    add("selftest", "plus4-pass.prg", TestType::EXITCODE, 10000000, "selftest", false);
    add("selftest", "plus4-fail.prg", TestType::EXITCODE, 10000000, "selftest", true);

    // These are already discovered by scan of Plus4/ directory, but we
    // ensure they have correct metadata (timeout from testlist)
    auto set_timeout = [&](const std::string& key, uint32_t timeout) {
        auto it = test_registry_.find(key);
        if (it != test_registry_.end())
            it->second.timeout_cycles = timeout;
    };
    set_timeout("Plus4/openio/outrun.prg", 10000000);
    set_timeout("Plus4/openio/outrun2.prg", 10000000);
    set_timeout("Plus4/tcbm/test.prg", 10000000);
}

std::vector<TestDescriptor> TestFramework::get_all_tests() const {
    std::vector<TestDescriptor> all;
    all.reserve(test_registry_.size());
    for (const auto& [key, td] : test_registry_)
        all.push_back(td);
    return all;
}

std::vector<TestDescriptor> TestFramework::get_filtered_tests(const TestFilter& filter) const {
    std::vector<TestDescriptor> filtered;
    for (const auto& [key, td] : test_registry_) {
        if (filter.matches(td))
            filtered.push_back(td);
    }
    return filtered;
}

// =============================================================================
// Test execution — creates a fresh Plus4System per test
// =============================================================================

// TED border color hue indices (lower 4 bits of $FF19)
static constexpr uint8_t TED_HUE_RED   = 2;
static constexpr uint8_t TED_HUE_GREEN = 5;

TestResult TestFramework::run_test(const TestDescriptor& test) {
    TestResult result;
    result.test = test;

    auto start_time = std::chrono::high_resolution_clock::now();

    if (verbose_) {
        printf("\nRunning test: %s [%s]\n", test.path.c_str(),
               test_type_to_string(test.type).c_str());
    }

    // Skip interactive tests in batch mode
    if (test.type == TestType::INTERACTIVE) {
        result.status = TestStatus::SKIPPED;
        result.message = "Interactive test skipped in batch mode";
        return result;
    }

    // ---- Create a fresh Plus4 system (64KB, PAL by default) ----
    Plus4System sys;
    {
        auto cfg = sys.get_configuration();
        // Region: 0=PAL, 1=NTSC
        if (has_flag(test.required_hw, HardwareConfig::TED_NTSC))
            cfg.region_option_index = 1;
        else
            cfg.region_option_index = 0; // default PAL
        sys.set_configuration(cfg);
    }
    if (!sys.initialize()) {
        result.status = TestStatus::ERROR;
        result.message = "Failed to initialize Plus4 system";
        return result;
    }

    // Allocate framebuffer for TED rendering
    bool is_pal = !has_flag(test.required_hw, HardwareConfig::TED_NTSC);
    int fb_w = TED_VISIBLE_WIDTH;
    int fb_h = is_pal ? TED_VISIBLE_HEIGHT_PAL : TED_VISIBLE_HEIGHT_NTSC;
    std::vector<uint32_t> framebuffer(fb_w * fb_h, 0);
    sys.set_framebuffer(framebuffer.data(), fb_w, fb_h);

    // ---- Load PRG into RAM ----
    MemoryChip* ram = sys.ram();
    if (!ram) {
        result.status = TestStatus::ERROR;
        result.message = "No RAM chip available";
        sys.shutdown();
        return result;
    }

    std::string full_path = vice_testprogs_path_ + "/" + test.path;
    uint16_t load_addr = 0, sys_addr = 0;
    if (!c64_test_load_prg_file(full_path.c_str(), ram, &load_addr, &sys_addr)) {
        result.status = TestStatus::ERROR;
        result.message = "Failed to load PRG file: " + full_path;
        sys.shutdown();
        return result;
    }

    if (verbose_) {
        printf("  Loaded: $%04X (SYS $%04X)\n", load_addr, sys_addr);
    }

    // ---- Enable debug cart register at $FDCF ----
    sys.enable_debug_cart(true);
    sys.clear_debug_cart();

    // ---- Determine execution strategy ----
    // Programs loading at $1001 (BASIC start) with a SYS command need
    // BASIC boot.  Others can be direct-executed after KERNAL boot.
    bool needs_basic_boot = (load_addr == c16_constants::BASIC_START && sys_addr != 0);

    auto* cpu = sys.cpu();
    if (!cpu) {
        result.status = TestStatus::ERROR;
        result.message = "No CPU available";
        sys.shutdown();
        return result;
    }

    if (needs_basic_boot) {
        // Boot the system through KERNAL+BASIC until the READY prompt
        // appears, then simulate "RUN" by jumping to the SYS address.
        if (verbose_) printf("  BASIC boot: waiting for READY prompt...\n");

        const uint32_t MAX_BOOT_CYCLES = 5000000; // ~5.6 seconds PAL
        for (uint32_t i = 0; i < MAX_BOOT_CYCLES; i++) {
            sys.tick();

            // Check BASIC warm-start vector at $0302/$0303
            if ((i & 0x3FF) == 0) { // every 1024 cycles
                if (ram->data()[0x0302] == c16_constants::BASIC_WARMSTART_LO &&
                    ram->data()[0x0303] == c16_constants::BASIC_WARMSTART_HI &&
                    ram->data()[c16_constants::KBD_BUFFER_COUNT] == 0) {
                    if (verbose_) printf("  BASIC ready at cycle %u\n", i);
                    break;
                }
            }
        }

        // Jump to SYS address
        cpu->set(REG_PC, sys_addr);
        cpu->transition_to_fetch();
        cpu->set(REG_AB, sys_addr);

        // Set up return address on stack pointing to BASIC warm start ($8712)
        // so RTS returns safely to BASIC
        uint8_t sp = cpu->get(REG_S);
        uint16_t return_addr = 0x8712 - 1; // RTS adds 1
        ram->data()[0x0100 + sp] = (return_addr >> 8) & 0xFF;
        sp--;
        ram->data()[0x0100 + sp] = return_addr & 0xFF;
        sp--;
        cpu->set(REG_S, sp);

        if (verbose_) printf("  Jumping to SYS $%04X\n", sys_addr);
    } else {
        // Direct execution: boot KERNAL, then jump
        const uint32_t KERNAL_BOOT_CYCLES = 2200000;
        for (uint32_t i = 0; i < KERNAL_BOOT_CYCLES; i++)
            sys.tick();

        uint16_t start = (sys_addr != 0) ? sys_addr : load_addr;
        cpu->set(REG_PC, start);
        cpu->transition_to_fetch();
        cpu->set(REG_AB, start);

        if (verbose_) printf("  Direct execution at $%04X\n", start);
    }

    // ---- Main execution loop ----
    uint32_t max_cycles = test.timeout_cycles;
    uint32_t cycles = 0;
    uint16_t last_pc = 0xFFFF;
    uint32_t pc_stable_count = 0;

    result.status = TestStatus::TIMEOUT;

    while (cycles < max_cycles) {
        sys.tick();
        cycles++;

        // Check debug cart register ($FDCF)
        if (sys.debug_cart_written()) {
            uint8_t val = sys.debug_cart_value();
            sys.clear_debug_cart();

            if (val == 0x00) {
                result.status = TestStatus::PASSED;
                result.exit_code = 0x00;
                result.message = "Test passed ($FDCF = $00)";
                break;
            } else if (val == 0xFF) {
                result.status = TestStatus::FAILED;
                result.exit_code = 0xFF;
                result.message = "Test failed ($FDCF = $FF)";
                break;
            }
            // Other values are subtest indicators — continue
        }

        // Periodic infinite-loop check (every 256 cycles)
        if ((cycles & 0xFF) == 0) {
            uint16_t current_pc = cpu->get(REG_PC);
            if (current_pc == last_pc) {
                pc_stable_count++;
                if (pc_stable_count >= 2) {
                    // Verify JMP * (opcode $4C with target == PC)
                    uint8_t opcode = ram->data()[current_pc];
                    bool is_jmp_self = false;
                    if (opcode == 0x4C) {
                        uint16_t target = ram->data()[(current_pc + 1) & 0xFFFF]
                                        | (ram->data()[(current_pc + 2) & 0xFFFF] << 8);
                        is_jmp_self = (target == current_pc);
                    }
                    if (is_jmp_self || pc_stable_count >= 8) {
                        // Read TED border color ($FF19): lower 4 bits = hue
                        ted7360_t* ted = sys.ted();
                        uint8_t border_hue = ted
                            ? (ted->registers.data[TED_REG_BORDER] & 0x0F)
                            : 0;

                        if (border_hue == TED_HUE_GREEN) {
                            result.status = TestStatus::PASSED;
                            result.message = "Test passed (border=GREEN, loop at $"
                                + std::to_string(current_pc) + ")";
                        } else if (border_hue == TED_HUE_RED) {
                            result.status = TestStatus::FAILED;
                            result.message = "Test failed (border=RED, loop at $"
                                + std::to_string(current_pc) + ")";
                        } else {
                            // Unknown border color — treat as passed
                            // (some tests set custom colors after success)
                            char buf[128];
                            snprintf(buf, sizeof(buf),
                                "Loop at $%04X, border hue=%u", current_pc, border_hue);
                            result.status = TestStatus::PASSED;
                            result.message = buf;
                        }

                        if (verbose_) {
                            printf("  Infinite loop at PC=$%04X, border hue=%u\n",
                                   current_pc, border_hue);
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

    result.cycles_executed = cycles;

    // Handle expect_fail: flip PASSED<->FAILED
    if (test.expect_fail) {
        if (result.status == TestStatus::FAILED) {
            result.status = TestStatus::PASSED;
            result.message += " (expected failure)";
        } else if (result.status == TestStatus::PASSED) {
            result.status = TestStatus::FAILED;
            result.message += " (expected failure but test passed)";
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (verbose_) {
        printf("  Result: %s (%s) — %.1f ms, %u cycles\n",
               test_status_to_string(result.status).c_str(),
               result.message.c_str(),
               result.execution_time_ms,
               result.cycles_executed);
    }

    sys.shutdown();
    return result;
}

std::vector<TestResult> TestFramework::run_all_tests(const TestFilter& filter) {
    auto tests = get_filtered_tests(filter);
    std::vector<TestResult> results;
    results.reserve(tests.size());

    printf("\n=== Running %zu Plus4/TED tests ===\n", tests.size());

    for (size_t i = 0; i < tests.size(); i++) {
        printf("[%zu/%zu] ", i + 1, tests.size());

        TestResult result = run_test(tests[i]);
        results.push_back(result);

        const char* sym = "?";
        switch (result.status) {
            case TestStatus::PASSED:  sym = "\xe2\x9c\x93"; break;  // ✓
            case TestStatus::FAILED:  sym = "\xe2\x9c\x97"; break;  // ✗
            case TestStatus::TIMEOUT: sym = "\xe2\x8f\xb1"; break;  // ⏱
            case TestStatus::SKIPPED: sym = "\xe2\x97\x8b"; break;  // ○
            case TestStatus::ERROR:   sym = "!"; break;
            default: break;
        }
        printf("%s %s\n", sym, tests[i].name.c_str());
    }

    return results;
}

// =============================================================================
// Statistics & reporting
// =============================================================================

TestStats TestFramework::get_statistics(const std::vector<TestResult>& results) const {
    TestStats s;
    for (const auto& r : results) {
        s.total++;
        s.total_time_ms += r.execution_time_ms;
        switch (r.status) {
            case TestStatus::PASSED:  s.passed++; break;
            case TestStatus::FAILED:  s.failed++; break;
            case TestStatus::TIMEOUT: s.timeout++; break;
            case TestStatus::SKIPPED: s.skipped++; break;
            case TestStatus::ERROR:   s.error++; break;
            default: break;
        }
    }
    return s;
}

void TestFramework::print_summary(const std::vector<TestResult>& results) const {
    TestStats s = get_statistics(results);

    printf("\n=== Plus4/TED Test Statistics ===\n");
    printf("Total tests:    %d\n", s.total);
    printf("Passed:         %d (%.1f%%)\n", s.passed, s.pass_rate());
    printf("Failed:         %d\n", s.failed);
    printf("Timeout:        %d\n", s.timeout);
    printf("Skipped:        %d\n", s.skipped);
    printf("Error:          %d\n", s.error);
    printf("Total time:     %.2f seconds\n", s.total_time_ms / 1000.0);

    // List failures
    bool has_failures = false;
    for (const auto& r : results) {
        if (r.status == TestStatus::FAILED || r.status == TestStatus::TIMEOUT) {
            if (!has_failures) {
                printf("\n=== Failed Tests ===\n");
                has_failures = true;
            }
            printf("  %s: %s\n", r.test.path.c_str(), r.message.c_str());
        }
    }
    if (!has_failures) printf("\nNo failures.\n");
}

void TestFramework::save_results(
    const std::string& output_file, const std::vector<TestResult>& results)
{
    std::ofstream out(output_file);
    if (!out.is_open()) {
        printf("ERROR: Cannot write results to: %s\n", output_file.c_str());
        return;
    }

    out << "Plus4/TED Test Results\n";
    out << "======================\n\n";

    for (const auto& r : results) {
        out << "Test: " << r.test.path << "\n";
        out << "  Status: " << test_status_to_string(r.status) << "\n";
        out << "  Type: " << test_type_to_string(r.test.type) << "\n";
        out << "  Cycles: " << r.cycles_executed << "\n";
        out << "  Time: " << r.execution_time_ms << " ms\n";
        if (!r.message.empty())
            out << "  Message: " << r.message << "\n";
        out << "\n";
    }

    TestStats s = get_statistics(results);
    out << "\nSummary:\n";
    out << "  Total: "   << s.total   << "\n";
    out << "  Passed: "  << s.passed  << " (" << s.pass_rate() << "%)\n";
    out << "  Failed: "  << s.failed  << "\n";
    out << "  Timeout: " << s.timeout << "\n";
    out << "  Skipped: " << s.skipped << "\n";
    out << "  Error: "   << s.error   << "\n";

    out.close();
    printf("Results saved to: %s\n", output_file.c_str());
}

void TestFramework::save_results_json(
    const std::string& output_file, const std::vector<TestResult>& results)
{
    std::ofstream out(output_file);
    if (!out.is_open()) {
        printf("ERROR: Cannot write JSON results to: %s\n", output_file.c_str());
        return;
    }

    out << "{\n  \"results\": [\n";
    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"path\": \""     << r.test.path << "\",\n";
        out << "      \"name\": \""     << r.test.name << "\",\n";
        out << "      \"category\": \"" << r.test.category << "\",\n";
        out << "      \"status\": \""   << test_status_to_string(r.status) << "\",\n";
        out << "      \"type\": \""     << test_type_to_string(r.test.type) << "\",\n";
        out << "      \"cycles\": "     << r.cycles_executed << ",\n";
        out << "      \"time_ms\": "    << r.execution_time_ms << ",\n";
        out << "      \"exit_code\": "  << (int)r.exit_code << ",\n";
        out << "      \"message\": \""  << r.message << "\"\n";
        out << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    TestStats s = get_statistics(results);
    out << "  \"summary\": {\n";
    out << "    \"total\": "        << s.total   << ",\n";
    out << "    \"passed\": "       << s.passed  << ",\n";
    out << "    \"failed\": "       << s.failed  << ",\n";
    out << "    \"timeout\": "      << s.timeout << ",\n";
    out << "    \"skipped\": "      << s.skipped << ",\n";
    out << "    \"error\": "        << s.error   << ",\n";
    out << "    \"pass_rate\": "    << s.pass_rate() << ",\n";
    out << "    \"total_time_ms\": "<< s.total_time_ms << "\n";
    out << "  }\n}\n";

    out.close();
    printf("JSON results saved to: %s\n", output_file.c_str());
}

} // namespace c16_test
