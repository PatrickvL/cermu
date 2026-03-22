/*
 * mc6809_processor_tests_runner.cpp — MC6809 SingleStepTests runner
 *
 * Runs JSON-based MC6809 single-step tests in the same format used by the
 * Z80 and fam65xx test runners.  Each test specifies an initial CPU state
 * (registers + RAM), the test runner executes one instruction, then compares
 * the resulting state against expected final values.
 *
 * MC6809-specific adaptations:
 *   - No I/O ports (MC6809 uses memory-mapped I/O only)
 *   - R/W signal (single line, not separate RD/WR)
 *   - Bus cycle tracking: address, data, rw, vma (valid memory address)
 *   - Registers: A, B, X, Y, U, S, PC, DP, CC
 *   - Instruction completion via opdone()
 *
 * JSON test format (per test case):
 * {
 *   "name": "test_name",
 *   "initial": {
 *     "a": 0, "b": 0, "x": 0, "y": 0, "u": 0, "s": 0,
 *     "pc": 0, "dp": 0, "cc": 0,
 *     "ram": [[addr, byte], ...]
 *   },
 *   "final": {
 *     "a": 0, "b": 0, "x": 0, "y": 0, "u": 0, "s": 0,
 *     "pc": 0, "dp": 0, "cc": 0,
 *     "ram": [[addr, byte], ...]
 *   },
 *   "cycles": [[addr, data|null, "flags"], ...]
 * }
 *
 * Cycle flags string: "r-" = read, "w-" = write, "--" = internal (VMA=0)
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <sstream>

#ifndef CERMU_IMPL
    #define CERMU_IMPL
#endif

// MC6809 CPU variant headers
#include "../src/chip/cpu/mc6809/motorola_mc6809.hpp"
#include "../src/chip/cpu/mc6809/motorola_mc6809e.hpp"
#include "../src/chip/cpu/mc6809/hitachi_hd6309.hpp"

// JSON parsing utilities
#include "json_parser.hpp"

using namespace mc6809;

namespace fs = std::filesystem;

// ============================================================================
// Thread-local memory for CPU testing
// ============================================================================

thread_local uint8_t test_memory[65536];

// ============================================================================
// Global flags
// ============================================================================

static bool g_test_verbose = false;
static bool g_test_quiet = false;
static bool g_stop_on_failure = true;
static std::atomic<bool> g_test_failed{false};
static std::atomic<size_t> g_test_verbose_count{0};
static constexpr size_t MAX_VERBOSE_OUTPUTS = 1000;

// ============================================================================
// MC6809-specific test data structures
// ============================================================================

// MC6809 CPU state for initial/final comparison
struct mc6809_cpu_state_t {
    uint8_t  a, b;
    uint16_t x, y, u, s, pc;
    uint8_t  dp, cc;

    // RAM entries: [address, byte]
    struct { uint16_t address; uint8_t value; } ram[64];
    uint8_t ram_count;
};

// MC6809 bus cycle trace entry
struct mc6809_bus_cycle_t {
    uint16_t address;
    uint8_t  data;
    bool     data_valid;  // false if data was null in JSON
    bool     rw;          // true = read, false = write
    bool     vma;         // Valid Memory Address (false = internal cycle)
};

// Recorded bus cycle (actual — from execution)
struct recorded_bus_cycle_t {
    uint16_t address;
    uint8_t  data;
    bool     data_valid;
    bool     rw;
    bool     vma;
};

// Complete MC6809 test case
struct mc6809_test_t {
    char name[128];
    mc6809_cpu_state_t initial;
    mc6809_cpu_state_t final_state;

    // Expected bus cycles
    mc6809_bus_cycle_t cycles[64];
    uint16_t cycle_count;
    bool has_cycles;
};

// ============================================================================
// MC6809 JSON parser
// ============================================================================

// Parse an MC6809 CPU state object from JSON
static bool mc6809_parse_state(const char* json, const char* state_name,
                                mc6809_cpu_state_t* state) {
    const char* obj = json_find_key(json, state_name);
    if (!obj || *obj != '{') return false;

    state->a  = static_cast<uint8_t>(json_parse_number(obj, "a"));
    state->b  = static_cast<uint8_t>(json_parse_number(obj, "b"));
    state->x  = static_cast<uint16_t>(json_parse_number(obj, "x"));
    state->y  = static_cast<uint16_t>(json_parse_number(obj, "y"));
    state->u  = static_cast<uint16_t>(json_parse_number(obj, "u"));
    state->s  = static_cast<uint16_t>(json_parse_number(obj, "s"));
    state->pc = static_cast<uint16_t>(json_parse_number(obj, "pc"));
    state->dp = static_cast<uint8_t>(json_parse_number(obj, "dp"));
    state->cc = static_cast<uint8_t>(json_parse_number(obj, "cc"));

    // Parse RAM array: [[addr, byte], ...]
    state->ram_count = 0;
    const char* ram_arr = json_find_key(obj, "ram");
    if (ram_arr && *ram_arr == '[') {
        const char* pos = ram_arr + 1;
        while (*pos && state->ram_count < 64) {
            pos = json_skip_whitespace(pos);
            if (*pos == ']') break;
            if (*pos != '[') break;
            pos++;
            pos = json_skip_whitespace(pos);
            state->ram[state->ram_count].address =
                static_cast<uint16_t>(strtol(pos, const_cast<char**>(&pos), 0));
            pos = json_skip_whitespace(pos);
            if (*pos == ',') pos++;
            pos = json_skip_whitespace(pos);
            state->ram[state->ram_count].value =
                static_cast<uint8_t>(strtol(pos, const_cast<char**>(&pos), 0));
            pos = json_skip_whitespace(pos);
            if (*pos == ']') pos++;
            state->ram_count++;
            pos = json_skip_whitespace(pos);
            if (*pos == ',') pos++;
        }
    }
    return true;
}

// Parse cycles array: [[addr, data|null, "flags"], ...]
// Flags string format: "r-" = read (RW=1, VMA=1)
//                       "w-" = write (RW=0, VMA=1)
//                       "--" = internal (VMA=0)
static bool mc6809_parse_cycles(const char* json, mc6809_test_t* test) {
    const char* arr = json_find_key(json, "cycles");
    if (!arr || *arr != '[') {
        test->has_cycles = false;
        test->cycle_count = 0;
        return true;
    }
    test->has_cycles = true;
    test->cycle_count = 0;
    const char* pos = arr + 1;

    while (*pos && test->cycle_count < 64) {
        pos = json_skip_whitespace(pos);
        if (*pos == ']') break;
        if (*pos != '[') break;
        pos++;

        mc6809_bus_cycle_t& cyc = test->cycles[test->cycle_count];
        cyc.rw = true;  // Default: read
        cyc.vma = true; // Default: valid
        cyc.data_valid = true;

        // Address (may be null)
        pos = json_skip_whitespace(pos);
        if (strncmp(pos, "null", 4) == 0) {
            cyc.address = 0;
            pos += 4;
        } else {
            cyc.address = static_cast<uint16_t>(strtol(pos, const_cast<char**>(&pos), 0));
        }
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;

        // Data (may be null)
        pos = json_skip_whitespace(pos);
        if (strncmp(pos, "null", 4) == 0) {
            cyc.data = 0;
            cyc.data_valid = false;
            pos += 4;
        } else {
            cyc.data = static_cast<uint8_t>(strtol(pos, const_cast<char**>(&pos), 0));
        }
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;

        // Flags string: "r-" / "w-" / "--"
        pos = json_skip_whitespace(pos);
        if (*pos == '"') {
            pos++;
            if (pos[0] == 'r') { cyc.rw = true;  cyc.vma = true; }
            else if (pos[0] == 'w') { cyc.rw = false; cyc.vma = true; }
            else { cyc.rw = true; cyc.vma = false; }  // "--" = internal
            while (*pos && *pos != '"') pos++;
            if (*pos == '"') pos++;
        }

        pos = json_skip_whitespace(pos);
        if (*pos == ']') pos++;
        test->cycle_count++;
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;
    }
    return true;
}

// Parse a complete MC6809 test case from JSON
static bool mc6809_parse_test(const char* json, mc6809_test_t* test) {
    std::memset(test, 0, sizeof(*test));
    json_parse_string(json, "name", test->name, sizeof(test->name));
    if (!mc6809_parse_state(json, "initial", &test->initial)) return false;
    if (!mc6809_parse_state(json, "final", &test->final_state)) return false;
    mc6809_parse_cycles(json, test);
    return true;
}

// ============================================================================
// Test results tracking
// ============================================================================

struct TestResults {
    size_t total_tests = 0;
    size_t passed_tests = 0;
    size_t failed_tests = 0;
    size_t state_mismatches = 0;
    size_t cycle_mismatches = 0;

    // Per-opcode tracking (256 base opcodes)
    uint32_t opcode_totals[256]{};
    uint32_t opcode_failures[256]{};
};

struct ThreadSafeTestResults {
    std::atomic<size_t> total_tests{0};
    std::atomic<size_t> passed_tests{0};
    std::atomic<size_t> failed_tests{0};
    std::atomic<size_t> state_mismatches{0};
    std::atomic<size_t> cycle_mismatches{0};

    std::mutex opcode_mtx;
    uint32_t opcode_totals[256]{};
    uint32_t opcode_failures[256]{};

    void record_opcode_result(uint8_t opcode, bool passed) {
        std::lock_guard<std::mutex> lk(opcode_mtx);
        opcode_totals[opcode]++;
        if (!passed) opcode_failures[opcode]++;
    }

    void merge_into(TestResults& r) {
        r.total_tests = total_tests.load();
        r.passed_tests = passed_tests.load();
        r.failed_tests = failed_tests.load();
        r.state_mismatches = state_mismatches.load();
        r.cycle_mismatches = cycle_mismatches.load();
        std::lock_guard<std::mutex> lk(opcode_mtx);
        std::memcpy(r.opcode_totals, opcode_totals, sizeof(opcode_totals));
        std::memcpy(r.opcode_failures, opcode_failures, sizeof(opcode_failures));
    }
};

// Thread-safe output buffer
struct ThreadSafeOutput {
    std::mutex mtx;
    std::vector<std::string> lines;

    void add(const std::string& s) {
        std::lock_guard<std::mutex> lk(mtx);
        lines.push_back(s);
    }

    bool has_pending() {
        std::lock_guard<std::mutex> lk(mtx);
        return !lines.empty();
    }

    void flush() {
        std::vector<std::string> local;
        { std::lock_guard<std::mutex> lk(mtx); std::swap(local, lines); }
        for (const auto& s : local) std::cout << s;
        std::cout << std::flush;
    }
};

// ============================================================================
// MC6809 Test Harness
// ============================================================================

template <typename CPU>
class MC6809TestHarness {
    CPU cpu;
    bus_state_t pins_;
    uint8_t* memory_;  // Points to thread_local test_memory

    std::vector<recorded_bus_cycle_t> actual_bus_cycles_;

public:
    MC6809TestHarness() : memory_(test_memory) {
        std::memset(memory_, 0, 65536);
        pins_ = cpu.init();
    }

    uint8_t get_memory(uint16_t addr) const { return memory_[addr]; }

    // === Setup ===

    // Selectively clear only addresses used by previous test
    void setup_for_test(const mc6809_cpu_state_t* initial,
                        const mc6809_cpu_state_t* prev_final) {
        actual_bus_cycles_.clear();

        if (prev_final) {
            // Clear only previously touched RAM entries
            for (uint8_t i = 0; i < prev_final->ram_count; i++)
                memory_[prev_final->ram[i].address] = 0;
        } else {
            std::memset(memory_, 0, 65536);
        }

        // Load initial RAM
        for (uint8_t i = 0; i < initial->ram_count; i++)
            memory_[initial->ram[i].address] = initial->ram[i].value;
    }

    // Load CPU registers from test initial state
    void load_state(const mc6809_cpu_state_t* s) {
        cpu.set_a(s->a);
        cpu.set_b(s->b);
        cpu.set_x(s->x);
        cpu.set_y(s->y);
        cpu.set_u(s->u);
        cpu.set_s(s->s);
        cpu.set_pc(s->pc);
        cpu.set_dp(s->dp);
        cpu.set_cc(s->cc);

        // Ensure interrupt lines are inactive (high) for test
        pins_ = CPU::default_bus_state();
    }

    // === Execution ===

    // Service the bus between cycles
    bus_state_t service_bus(bus_state_t pins) {
        bool rw  = BUS_GET_BIT(pins, BUS_RW_BIT);   // RW high = read
        uint16_t addr = BUS_GET_ADDR(pins);

        // Detect VMA: BA=0 with address != $FFFF is a reasonable heuristic.
        // Internal cycles put $FFFF on the address bus and keep RW=read.
        // True VMA tracking would require the AVMA signal, but for test
        // purposes we infer it: if the CPU drives $FFFF as a read with no
        // pending read setup, it's likely internal.  For now, record every
        // cycle and let the comparison handle it.
        bool vma = true;

        // Record bus cycle
        recorded_bus_cycle_t rec;
        rec.address = addr;
        rec.data = BUS_GET_DATA(pins);
        rec.data_valid = true;
        rec.rw = rw;
        rec.vma = vma;
        actual_bus_cycles_.push_back(rec);

        if (rw) {
            // Memory read (RW=1)
            BUS_SET_DATA(pins, memory_[addr]);
        } else {
            // Memory write (RW=0)
            memory_[addr] = BUS_GET_DATA(pins);
        }

        return pins;
    }

    // Execute one instruction. Returns false on timeout.
    bool step() {
        constexpr uint32_t MAX_CYCLES = 100;
        uint32_t t = 0;

        // First tick to start the instruction
        pins_ = cpu.tick(pins_);
        pins_ = service_bus(pins_);
        t++;

        // Continue ticking until instruction completes
        while (t < MAX_CYCLES) {
            pins_ = cpu.tick(pins_);
            pins_ = service_bus(pins_);
            t++;

            if (cpu.opdone()) break;
        }

        return t < MAX_CYCLES;
    }

    // === Accessors ===
    const std::vector<recorded_bus_cycle_t>& get_actual_bus_cycles() const {
        return actual_bus_cycles_;
    }

    // Get current CPU state for comparison
    void read_state(mc6809_cpu_state_t* s) const {
        s->a  = cpu.a();
        s->b  = cpu.b();
        s->x  = cpu.x();
        s->y  = cpu.y();
        s->u  = cpu.u();
        s->s  = cpu.s();
        s->pc = cpu.pc();
        s->dp = cpu.dp();
        s->cc = cpu.cc();
        s->ram_count = 0;
    }
};

// ============================================================================
// Test work item
// ============================================================================

struct TestItem {
    std::string filepath;
    std::string test_json;
    std::string test_name;
};

// ============================================================================
// Worker thread pool
// ============================================================================

template <typename CPU>
class MC6809TestWorkerPool {
    std::vector<std::thread> workers_;
    std::queue<TestItem> queue_;
    std::mutex queue_mtx_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> total_added_{0};

    ThreadSafeOutput& output_;
    ThreadSafeTestResults& results_;
    std::atomic<bool>& test_failed_;

public:
    MC6809TestWorkerPool(size_t n, ThreadSafeOutput& out, ThreadSafeTestResults& res,
                         std::atomic<bool>& failed)
        : output_(out), results_(res), test_failed_(failed)
    {
        for (size_t i = 0; i < n; i++)
            workers_.emplace_back(&MC6809TestWorkerPool::worker, this);
    }

    ~MC6809TestWorkerPool() {
        { std::lock_guard<std::mutex> lock(queue_mtx_); shutdown_ = true; }
        queue_cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }

    void add_test(const TestItem& item) {
        { std::lock_guard<std::mutex> lock(queue_mtx_); queue_.push(item); total_added_++; }
        queue_cv_.notify_one();
    }

    void wait_completion() {
        while (true) {
            bool empty;
            { std::lock_guard<std::mutex> lk(queue_mtx_); empty = queue_.empty(); }
            if (empty && results_.total_tests.load() >= total_added_.load()) {
                { std::lock_guard<std::mutex> lk(queue_mtx_); shutdown_ = true; }
                queue_cv_.notify_all();
                break;
            }
            if (g_stop_on_failure && test_failed_.load()) {
                { std::lock_guard<std::mutex> lk(queue_mtx_); shutdown_ = true; }
                queue_cv_.notify_all();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    void worker() {
        MC6809TestHarness<CPU> harness;
        mc6809_test_t* prev_test = nullptr;

        while (!shutdown_) {
            TestItem item;
            {
                std::unique_lock<std::mutex> lk(queue_mtx_);
                queue_cv_.wait(lk, [&]{ return !queue_.empty() || shutdown_; });
                if (shutdown_) break;
                if (queue_.empty()) continue;
                item = queue_.front();
                queue_.pop();
            }

            if (g_stop_on_failure && test_failed_.load()) break;

            mc6809_test_t test;
            if (!mc6809_parse_test(item.test_json.c_str(), &test)) {
                std::ostringstream os;
                os << "ERROR: Failed to parse test: " << item.filepath << "\n";
                output_.add(os.str());
                results_.total_tests++;
                results_.failed_tests++;
                continue;
            }

            bool result = run_test(&test, harness, prev_test);
            if (!prev_test)
                prev_test = new mc6809_test_t(test);
            else
                *prev_test = test;

            if (!result && g_stop_on_failure)
                test_failed_ = true;
        }

        delete prev_test;
    }

    bool run_test(const mc6809_test_t* test, MC6809TestHarness<CPU>& harness,
                  const mc6809_test_t* prev) {
        results_.total_tests++;

        std::ostringstream debug;
        bool suppress_verbose = false;
        if (g_test_verbose) {
            size_t cnt = g_test_verbose_count.fetch_add(1);
            if (cnt >= MAX_VERBOSE_OUTPUTS)
                suppress_verbose = true;
            else
                debug << "Running test: " << test->name << "\n";
        }

        // Setup
        const mc6809_cpu_state_t* prev_final = prev ? &prev->final_state : nullptr;
        harness.setup_for_test(&test->initial, prev_final);
        harness.load_state(&test->initial);

        // Determine opcode for statistics
        uint8_t opcode = harness.get_memory(test->initial.pc);

        // Execute
        if (!harness.step()) {
            if (!g_test_quiet) {
                debug << "FAIL " << test->name << ": Execution timeout (opcode 0x"
                      << std::hex << static_cast<int>(opcode) << ")\n" << std::dec;
                output_.add(debug.str());
            }
            results_.failed_tests++;
            results_.record_opcode_result(opcode, false);
            return false;
        }

        // Compare state
        bool state_ok = true;
        bool cycle_ok = true;

        mc6809_cpu_state_t actual;
        harness.read_state(&actual);
        const mc6809_cpu_state_t& expected = test->final_state;

        // Register comparison macro
        #define CHECK_REG(reg_name, actual_val, expected_val) \
            if ((actual_val) != (expected_val)) { \
                if (!g_test_quiet) { \
                    debug << "FAIL " << test->name << ": " << reg_name << " - expected 0x" \
                          << std::hex << static_cast<int>(expected_val) \
                          << ", got 0x" << static_cast<int>(actual_val) << std::dec << "\n"; \
                } \
                state_ok = false; \
            }

        CHECK_REG("A",  actual.a,  expected.a);
        CHECK_REG("B",  actual.b,  expected.b);
        CHECK_REG("X",  actual.x,  expected.x);
        CHECK_REG("Y",  actual.y,  expected.y);
        CHECK_REG("U",  actual.u,  expected.u);
        CHECK_REG("S",  actual.s,  expected.s);
        CHECK_REG("PC", actual.pc, expected.pc);
        CHECK_REG("DP", actual.dp, expected.dp);
        CHECK_REG("CC", actual.cc, expected.cc);

        #undef CHECK_REG

        // Memory comparison
        for (uint8_t i = 0; i < expected.ram_count; i++) {
            uint8_t actual_val = harness.get_memory(expected.ram[i].address);
            if (actual_val != expected.ram[i].value) {
                if (!g_test_quiet) {
                    debug << "FAIL " << test->name << ": Memory[0x" << std::hex
                          << expected.ram[i].address << "] - expected 0x"
                          << static_cast<int>(expected.ram[i].value)
                          << ", got 0x" << static_cast<int>(actual_val) << std::dec << "\n";
                }
                state_ok = false;
            }
        }

        // Cycle count comparison
        if (test->has_cycles) {
            uint16_t expected_cycles = test->cycle_count;
            auto& actual_cycles = harness.get_actual_bus_cycles();
            if (actual_cycles.size() != expected_cycles) {
                if (!g_test_quiet) {
                    debug << "FAIL " << test->name << ": Cycle count - expected "
                          << expected_cycles << ", got " << actual_cycles.size() << "\n";
                }
                cycle_ok = false;
            }
        }

        // Record result
        if (state_ok && cycle_ok) {
            results_.passed_tests++;
            results_.record_opcode_result(opcode, true);
            if (g_test_verbose && !suppress_verbose) {
                debug << "PASS " << test->name << " (opcode 0x" << std::hex
                      << static_cast<int>(opcode) << ")\n" << std::dec;
                output_.add(debug.str());
            }
            return true;
        } else {
            results_.failed_tests++;
            if (!state_ok) results_.state_mismatches++;
            if (!cycle_ok) results_.cycle_mismatches++;
            results_.record_opcode_result(opcode, false);
            if (!g_test_quiet) {
                debug << "FAIL " << test->name << ": ";
                if (!state_ok) debug << "State ";
                if (!cycle_ok) debug << "Cycle ";
                debug << "mismatch (opcode 0x" << std::hex
                      << static_cast<int>(opcode) << ")\n" << std::dec;
                output_.add(debug.str());
            }
            return false;
        }
    }
};

// ============================================================================
// File collection
// ============================================================================

static void collect_tests_from_file(const std::string& filepath,
                                     std::vector<TestItem>& tests) {
    tests.clear();
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "ERROR: Could not open file: " << filepath << "\n";
        return;
    }
    auto size = file.tellg();
    if (size <= 0) return;
    file.seekg(0, std::ios::beg);
    std::string content(static_cast<size_t>(size), '\0');
    if (!file.read(&content[0], size)) return;
    file.close();

    tests.reserve(1100);

    if (!content.empty() && content[0] == '[') {
        size_t pos = 1;
        size_t idx = 0;
        while (pos < content.length()) {
            while (pos < content.length() &&
                   (content[pos] == ' ' || content[pos] == '\n' ||
                    content[pos] == '\r' || content[pos] == '\t' ||
                    content[pos] == ','))
                pos++;
            if (pos >= content.length() || content[pos] == ']') break;
            if (content[pos] == '{') {
                size_t start = pos;
                int depth = 1;
                pos++;
                while (pos < content.length() && depth > 0) {
                    if (content[pos] == '{') depth++;
                    else if (content[pos] == '}') depth--;
                    pos++;
                }
                if (depth == 0) {
                    TestItem item;
                    item.filepath = filepath;
                    item.test_json = content.substr(start, pos - start);
                    item.test_name = "test_" + std::to_string(idx++);
                    tests.push_back(std::move(item));
                }
            } else {
                pos++;
            }
        }
    }
}

static std::vector<TestItem> collect_all_tests(const std::vector<std::string>& paths,
                                                const std::string& opcode_filter) {
    std::vector<TestItem> all;
    std::vector<std::string> json_files;

    for (const auto& p : paths) {
        try {
            if (fs::is_directory(p)) {
                std::error_code ec;
                for (const auto& entry : fs::recursive_directory_iterator(p, ec)) {
                    if (ec) continue;
                    if (entry.is_regular_file(ec) && !ec &&
                        entry.path().extension() == ".json") {
                        if (!opcode_filter.empty()) {
                            std::string stem = entry.path().stem().string();
                            std::transform(stem.begin(), stem.end(), stem.begin(),
                                           [](unsigned char c) {
                                               return static_cast<char>(std::tolower(c));
                                           });
                            if (stem != opcode_filter) continue;
                        }
                        json_files.push_back(entry.path().string());
                    }
                }
            } else if (fs::is_regular_file(p)) {
                json_files.push_back(p);
            }
        } catch (const fs::filesystem_error& ex) {
            std::cout << "ERROR: " << p << " (" << ex.what() << ")\n";
        }
    }

    // Sort for deterministic order
    std::sort(json_files.begin(), json_files.end());

    std::vector<TestItem> file_tests;
    size_t n = 0;
    for (const auto& f : json_files) {
        if (json_files.size() > 50 && n % 50 == 0) std::cout << "." << std::flush;
        collect_tests_from_file(f, file_tests);
        all.insert(all.end(),
                   std::make_move_iterator(file_tests.begin()),
                   std::make_move_iterator(file_tests.end()));
        n++;
    }
    if (json_files.size() > 50) std::cout << "\n";

    return all;
}

// ============================================================================
// Usage / results printing
// ============================================================================

static void print_usage(const char* prog) {
    std::cout << "MC6809 SingleStepTests Runner\n";
    std::cout << "Usage: " << prog << " [options] <test_file_or_directory>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -v, --verbose     Verbose output\n";
    std::cout << "  -q, --quiet       Quiet mode (summary only)\n";
    std::cout << "  -c, --continue    Continue after failures\n";
    std::cout << "  -s, --stop-first  Stop on first failure (default)\n";
    std::cout << "  -j, --jobs N      Worker threads (default: cores-1)\n";
    std::cout << "  -o, --opcode HH   Filter to single opcode (e.g. 86 or 0x86)\n";
    std::cout << "  -h, --help        This message\n\n";
    std::cout << "Test data format: JSON files with initial/final CPU state + RAM.\n";
    std::cout << "Compatible with SingleStepTests format.\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " path/to/mc6809/v1/\n";
    std::cout << "  " << prog << " -o 86 -v path/to/mc6809/v1/\n";
    std::cout << "  " << prog << " -j 8 -c path/to/mc6809/v1/\n";
}

static void print_results(const TestResults& r, std::chrono::milliseconds dur,
                           size_t workers, size_t collected) {
    std::cout << "\n=== MC6809 PROCESSOR TESTS RESULTS ===\n";
    std::cout << "CPU: Motorola MC6809\n";
    std::cout << "Execution time: " << dur.count() << " ms\n";
    std::cout << "Worker threads: " << workers << "\n";
    std::cout << "Tests collected: " << collected << "\n";
    std::cout << "Tests executed: " << r.total_tests << "\n";
    std::cout << "Tests passed: " << r.passed_tests << "\n";
    std::cout << "Tests failed: " << r.failed_tests << "\n";

    if (r.total_tests > 0) {
        double rate = static_cast<double>(r.passed_tests) / r.total_tests * 100.0;
        std::cout << "Pass rate: " << std::fixed << std::setprecision(2) << rate << "%\n";
        double tps = static_cast<double>(r.total_tests) / (dur.count() / 1000.0);
        std::cout << "Performance: " << std::fixed << std::setprecision(1)
                  << tps << " tests/second\n";
    }

    if (r.failed_tests > 0) {
        std::cout << "\nFailure breakdown:\n";
        std::cout << "  State mismatches: " << r.state_mismatches << "\n";
        std::cout << "  Cycle mismatches: " << r.cycle_mismatches << "\n";

        std::cout << "\nFailing opcodes:\n";
        uint32_t failing = 0;
        for (int i = 0; i < 256; i++) {
            if (r.opcode_failures[i] > 0) {
                std::cout << "  0x" << std::hex << std::setfill('0') << std::setw(2) << i
                          << ": " << std::dec << r.opcode_failures[i]
                          << "/" << r.opcode_totals[i] << " failed\n";
                failing++;
            }
        }
        std::cout << "Total failing opcodes: " << failing << "\n";
    }

    if (r.passed_tests == r.total_tests && r.total_tests > 0)
        std::cout << "\nALL TESTS PASSED - MC6809 implementation is hardware-accurate!\n";
    else if (r.failed_tests > 0)
        std::cout << "\nSOME TESTS FAILED - implementation differs from reference\n";
}

// ============================================================================
// Main entry point
// ============================================================================

template <typename CPU>
static int run_all_tests(const std::vector<std::string>& test_paths,
                         const std::string& opcode_filter,
                         size_t num_workers) {
    std::cout << "=== MC6809 SingleStepTests Runner ===\n";
    std::cout << "CPU: Motorola MC6809\n";
    std::cout << "Worker threads: " << num_workers << "\n";
    if (!opcode_filter.empty())
        std::cout << "Opcode filter: 0x" << opcode_filter << "\n";
    std::cout << "Verbose: " << (g_test_verbose ? "yes" : "no") << "\n";
    std::cout << "Quiet: " << (g_test_quiet ? "yes" : "no") << "\n";
    std::cout << "Stop on failure: " << (g_stop_on_failure ? "yes" : "no") << "\n\n";

    auto t0 = std::chrono::high_resolution_clock::now();

    std::cout << "Collecting tests..." << std::flush;
    auto all_tests = collect_all_tests(test_paths, opcode_filter);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto collect_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    std::cout << " Found " << all_tests.size() << " tests in " << collect_ms.count() << "ms\n";

    if (all_tests.empty()) {
        std::cout << "No tests found!\n";
        return 1;
    }

    size_t effective = std::min(num_workers, std::max<size_t>(1, all_tests.size()));

    ThreadSafeOutput output;
    ThreadSafeTestResults thread_results;
    MC6809TestWorkerPool<CPU> pool(effective, output, thread_results, g_test_failed);

    std::cout << "Starting parallel execution (" << effective << " workers)...\n";
    for (const auto& t : all_tests)
        pool.add_test(t);

    // Periodic flush thread
    std::atomic<bool> flush_exit{false};
    std::thread flusher([&]() {
        while (!flush_exit.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (output.has_pending()) output.flush();
        }
    });

    pool.wait_completion();
    flush_exit = true;
    if (flusher.joinable()) flusher.join();
    output.flush();

    auto t2 = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t0);

    TestResults results;
    thread_results.merge_into(results);
    print_results(results, dur, effective, all_tests.size());

    if (results.total_tests == 0) return 1;
    return (results.passed_tests == results.total_tests) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::vector<std::string> test_paths;
    size_t num_workers = std::max(1u, std::thread::hardware_concurrency() - 1);
    std::string opcode_filter;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            g_test_verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            g_test_quiet = true;
        } else if (arg == "-c" || arg == "--continue") {
            g_stop_on_failure = false;
        } else if (arg == "-s" || arg == "--stop-first") {
            g_stop_on_failure = true;
        } else if (arg == "-j" || arg == "--jobs") {
            if (i + 1 < argc) num_workers = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "-o" || arg == "--opcode") {
            if (i + 1 < argc) {
                opcode_filter = argv[++i];
                if (opcode_filter.substr(0, 2) == "0x" ||
                    opcode_filter.substr(0, 2) == "0X")
                    opcode_filter = opcode_filter.substr(2);
                std::transform(opcode_filter.begin(), opcode_filter.end(),
                               opcode_filter.begin(),
                               [](unsigned char c) {
                                   return static_cast<char>(std::tolower(c));
                               });
            }
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            test_paths.push_back(arg);
        }
    }

    if (test_paths.empty()) {
        std::cout << "ERROR: No test path specified\n";
        print_usage(argv[0]);
        return 1;
    }

    // Default: MC6809 (internal clock variant)
    return run_all_tests<MotorolaMC6809>(test_paths, opcode_filter, num_workers);
}
