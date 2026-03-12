/*
 * z80_processor_tests_runner.cpp — Z80 SingleStepTests runner
 *
 * Runs the JSON-based Z80 single-step tests from:
 *   https://github.com/SingleStepTests/z80
 *
 * Follows the ProcessorTestHarness pattern established by the fam65xx
 * processor tests runner: thread-local memory, multi-threaded worker pool,
 * bus-cycle tracing, per-opcode statistics.
 *
 * Z80-specific adaptations:
 *   - T-state stepping (tick() = 1 T-state, not 1 clock cycle)
 *   - I/O port tracking (IN/OUT via IORQ signal)
 *   - Z80 bus signals: MREQ, IORQ, M1, RFSH, HALT, WAIT
 *   - Shadow registers (AF', BC', DE', HL')
 *   - WZ (MEMPTR) internal register
 *   - Instruction completion via opdone()
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
#include <unordered_map>
#include <stdexcept>

// Low-level JSON utilities (shared with fam65xx runner)
#include "json_parser.h"

#ifndef CERMU_IMPL
    #define CERMU_IMPL
#endif

// Z80 CPU variant headers
#include "../src/chip/cpu/z80/zilog_z80.h"
#include "../src/chip/cpu/z80/zilog_z80a.h"
#include "../src/chip/cpu/z80/zilog_z80b.h"
#include "../src/chip/cpu/z80/u880.h"

using namespace z80;

namespace fs = std::filesystem;

// ============================================================================
// Thread-local memory for CPU testing
// ============================================================================

thread_local uint8_t test_memory[65536];

// ============================================================================
// Global flags
// ============================================================================

static bool g_verbose = false;
static bool g_quiet = false;
static bool g_stop_on_failure = true;
static std::atomic<bool> g_test_failed{false};
static std::atomic<size_t> g_verbose_count{0};
static constexpr size_t MAX_VERBOSE_OUTPUTS = 1000;

// ============================================================================
// Z80-specific test data structures
// ============================================================================

// I/O port transaction recorded during test execution
struct z80_port_access_t {
    uint16_t address;   // Full 16-bit I/O address
    uint8_t  data;
    bool     is_write;  // true = OUT, false = IN
};

// Z80 CPU state for initial/final comparison
struct z80_cpu_state_t {
    uint16_t pc;
    uint16_t sp;
    uint8_t  a, b, c, d, e, f, h, l;
    uint8_t  i, r;
    uint16_t ix, iy;
    uint16_t wz;
    uint16_t af_, bc_, de_, hl_;
    uint8_t  im;
    bool     iff1, iff2;
    bool     ei;    // EI pending
    bool     p;     // LD A,I / LD A,R tracking
    bool     q;     // Flag modification tracking

    // RAM entries: [address, byte]
    struct { uint16_t address; uint8_t value; } ram[64];
    uint8_t ram_count;
};

// Z80 bus cycle trace entry
struct z80_bus_cycle_t {
    uint16_t address;
    uint8_t  data;
    bool     data_valid;  // false if data was null in JSON
    bool     rd;          // Read signal
    bool     wr;          // Write signal
    bool     mreq;        // Memory request
    bool     iorq;        // I/O request
};

// Complete Z80 test case
struct z80_test_t {
    char name[128];
    z80_cpu_state_t initial;
    z80_cpu_state_t final_state;

    // Expected bus cycles
    z80_bus_cycle_t cycles[128];
    uint16_t cycle_count;
    bool has_cycles;

    // Expected I/O port transactions
    z80_port_access_t ports[16];
    uint8_t port_count;
    bool has_ports;
};

// ============================================================================
// Z80 JSON parser (Z80-specific format)
// ============================================================================

// Parse a Z80 CPU state object from JSON
static bool z80_parse_state(const char* json, const char* state_name, z80_cpu_state_t* state) {
    const char* obj = json_find_key(json, state_name);
    if (!obj || *obj != '{') return false;

    state->pc  = static_cast<uint16_t>(json_parse_number(obj, "pc"));
    state->sp  = static_cast<uint16_t>(json_parse_number(obj, "sp"));
    state->a   = static_cast<uint8_t>(json_parse_number(obj, "a"));
    state->b   = static_cast<uint8_t>(json_parse_number(obj, "b"));
    state->c   = static_cast<uint8_t>(json_parse_number(obj, "c"));
    state->d   = static_cast<uint8_t>(json_parse_number(obj, "d"));
    state->e   = static_cast<uint8_t>(json_parse_number(obj, "e"));
    state->f   = static_cast<uint8_t>(json_parse_number(obj, "f"));
    state->h   = static_cast<uint8_t>(json_parse_number(obj, "h"));
    state->l   = static_cast<uint8_t>(json_parse_number(obj, "l"));
    state->i   = static_cast<uint8_t>(json_parse_number(obj, "i"));
    state->r   = static_cast<uint8_t>(json_parse_number(obj, "r"));
    state->ix  = static_cast<uint16_t>(json_parse_number(obj, "ix"));
    state->iy  = static_cast<uint16_t>(json_parse_number(obj, "iy"));
    state->wz  = static_cast<uint16_t>(json_parse_number(obj, "wz"));
    state->af_ = static_cast<uint16_t>(json_parse_number(obj, "af_"));
    state->bc_ = static_cast<uint16_t>(json_parse_number(obj, "bc_"));
    state->de_ = static_cast<uint16_t>(json_parse_number(obj, "de_"));
    state->hl_ = static_cast<uint16_t>(json_parse_number(obj, "hl_"));
    state->im  = static_cast<uint8_t>(json_parse_number(obj, "im"));
    state->iff1 = json_parse_number(obj, "iff1") != 0;
    state->iff2 = json_parse_number(obj, "iff2") != 0;
    state->ei   = json_parse_number(obj, "ei") != 0;
    state->p    = json_parse_number(obj, "p") != 0;
    state->q    = json_parse_number(obj, "q") != 0;

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
            state->ram[state->ram_count].address = static_cast<uint16_t>(strtol(pos, const_cast<char**>(&pos), 0));
            pos = json_skip_whitespace(pos);
            if (*pos == ',') pos++;
            pos = json_skip_whitespace(pos);
            state->ram[state->ram_count].value = static_cast<uint8_t>(strtol(pos, const_cast<char**>(&pos), 0));
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
// Flags string: positions are r=read, w=write, m=mreq, i=iorq
static bool z80_parse_cycles(const char* json, z80_test_t* test) {
    const char* arr = json_find_key(json, "cycles");
    if (!arr || *arr != '[') {
        test->has_cycles = false;
        test->cycle_count = 0;
        return true;
    }
    test->has_cycles = true;
    test->cycle_count = 0;
    const char* pos = arr + 1;

    while (*pos && test->cycle_count < 128) {
        pos = json_skip_whitespace(pos);
        if (*pos == ']') break;
        if (*pos != '[') break;
        pos++;

        z80_bus_cycle_t& cyc = test->cycles[test->cycle_count];
        cyc.rd = cyc.wr = cyc.mreq = cyc.iorq = false;
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

        // Flags string: "r-m-" / "--mi" / "w-m-" etc.
        pos = json_skip_whitespace(pos);
        if (*pos == '"') {
            pos++;
            // 4-char flag string: [r/w/-][?/-][m/-][i/-]
            if (pos[0] == 'r') cyc.rd = true;
            if (pos[0] == 'w') cyc.wr = true;
            if (pos[2] == 'm') cyc.mreq = true;
            if (pos[3] == 'i') cyc.iorq = true;
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

// Parse ports array: [[addr16, data, "r"|"w"], ...]
static bool z80_parse_ports(const char* json, z80_test_t* test) {
    const char* arr = json_find_key(json, "ports");
    if (!arr || *arr != '[') {
        test->has_ports = false;
        test->port_count = 0;
        return true;
    }
    test->has_ports = true;
    test->port_count = 0;
    const char* pos = arr + 1;

    while (*pos && test->port_count < 16) {
        pos = json_skip_whitespace(pos);
        if (*pos == ']') break;
        if (*pos != '[') break;
        pos++;

        z80_port_access_t& pa = test->ports[test->port_count];

        pos = json_skip_whitespace(pos);
        pa.address = static_cast<uint16_t>(strtol(pos, const_cast<char**>(&pos), 0));
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;

        pos = json_skip_whitespace(pos);
        pa.data = static_cast<uint8_t>(strtol(pos, const_cast<char**>(&pos), 0));
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;

        pos = json_skip_whitespace(pos);
        if (*pos == '"') {
            pos++;
            pa.is_write = (*pos == 'w');
            while (*pos && *pos != '"') pos++;
            if (*pos == '"') pos++;
        }

        pos = json_skip_whitespace(pos);
        if (*pos == ']') pos++;
        test->port_count++;
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;
    }
    return true;
}

// Parse a complete Z80 test case from JSON
static bool z80_parse_test(const char* json, z80_test_t* test) {
    std::memset(test, 0, sizeof(*test));
    json_parse_string(json, "name", test->name, sizeof(test->name));
    if (!z80_parse_state(json, "initial", &test->initial)) return false;
    if (!z80_parse_state(json, "final", &test->final_state)) return false;
    z80_parse_cycles(json, test);
    z80_parse_ports(json, test);
    return true;
}

// ============================================================================
// Test results tracking
// ============================================================================

struct TestResults {
    uint32_t total_tests = 0;
    uint32_t passed_tests = 0;
    uint32_t failed_tests = 0;
    uint32_t state_mismatches = 0;
    uint32_t cycle_mismatches = 0;
    uint32_t port_mismatches = 0;
    uint32_t opcode_failures[256] = {};
    uint32_t opcode_totals[256] = {};
};

struct ThreadSafeTestResults {
    std::atomic<uint32_t> total_tests{0};
    std::atomic<uint32_t> passed_tests{0};
    std::atomic<uint32_t> failed_tests{0};
    std::atomic<uint32_t> state_mismatches{0};
    std::atomic<uint32_t> cycle_mismatches{0};
    std::atomic<uint32_t> port_mismatches{0};

    std::mutex opcode_mutex;
    uint32_t opcode_failures[256] = {};
    uint32_t opcode_totals[256] = {};

    void record_opcode_result(uint8_t opcode_val, bool success) {
        std::lock_guard<std::mutex> lock(opcode_mutex);
        opcode_totals[opcode_val]++;
        if (!success) opcode_failures[opcode_val]++;
    }

    void merge_into(TestResults& r) {
        r.total_tests = total_tests.load();
        r.passed_tests = passed_tests.load();
        r.failed_tests = failed_tests.load();
        r.state_mismatches = state_mismatches.load();
        r.cycle_mismatches = cycle_mismatches.load();
        r.port_mismatches = port_mismatches.load();
        std::lock_guard<std::mutex> lock(opcode_mutex);
        std::memcpy(r.opcode_failures, opcode_failures, sizeof(opcode_failures));
        std::memcpy(r.opcode_totals, opcode_totals, sizeof(opcode_totals));
    }
};

// Thread-safe output buffer
class ThreadSafeOutput {
    std::mutex mtx_;
    std::queue<std::string> queue_;
    std::atomic<bool> pending_{false};
public:
    void add(const std::string& s) {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(s);
        pending_ = true;
    }
    void flush() {
        std::lock_guard<std::mutex> lock(mtx_);
        while (!queue_.empty()) {
            std::cout << queue_.front();
            queue_.pop();
        }
        pending_ = false;
    }
    bool has_pending() const { return pending_.load(); }
};

// ============================================================================
// Z80 Test Harness — templated on the Z80 variant
// ============================================================================

template <typename CPU>
class Z80TestHarness {
public:
    CPU cpu;

private:
    uint8_t* memory_;
    uint32_t t_state_count_;
    bus_state_t pins_;

    // I/O port backing store (256 ports, only low byte addressed for tests)
    uint8_t io_ports_[65536];

    // Recorded I/O port transactions during test execution
    std::vector<z80_port_access_t> actual_ports_;

    // Edge detection: only record port access on the first T-state where IORQ becomes active
    bool prev_iorq_active_;

    // Recorded bus cycles
    struct recorded_bus_cycle_t {
        uint16_t address;
        uint8_t  data;
        bool     data_valid;
        bool     rd, wr, mreq, iorq;
    };
    std::vector<recorded_bus_cycle_t> actual_bus_cycles_;

public:
    Z80TestHarness()
        : memory_(test_memory), t_state_count_(0), pins_(CPU::default_bus_state()), prev_iorq_active_(false) {
        std::fill(memory_, memory_ + 65536, static_cast<uint8_t>(0));
        std::memset(io_ports_, 0, sizeof(io_ports_));
        cpu.init();
        actual_bus_cycles_.reserve(32);
        actual_ports_.reserve(4);
    }

    // === Memory access ===
    void set_memory(uint16_t addr, uint8_t data) { memory_[addr] = data; }
    uint8_t get_memory(uint16_t addr) const { return memory_[addr]; }
    void clear_memory(uint16_t addr) { memory_[addr] = 0; }

    // === I/O port access ===
    void set_io_port(uint16_t addr, uint8_t data) { io_ports_[addr] = data; }

    // === Setup ===
    void setup_for_test(const z80_cpu_state_t* initial, const z80_cpu_state_t* prev_final) {
        actual_bus_cycles_.clear();
        actual_bus_cycles_.reserve(32);
        actual_ports_.clear();
        actual_ports_.reserve(4);

        // Selective memory clearing
        if (prev_final) {
            for (uint8_t i = 0; i < prev_final->ram_count; i++)
                memory_[prev_final->ram[i].address] = 0;
            // Also clear from actual bus cycle writes
            for (const auto& cyc : actual_bus_cycles_) {
                if (cyc.wr && cyc.mreq) memory_[cyc.address] = 0;
            }
        } else {
            std::fill(memory_, memory_ + 65536, static_cast<uint8_t>(0));
        }

        std::memset(io_ports_, 0, sizeof(io_ports_));

        // Load initial RAM
        for (uint8_t i = 0; i < initial->ram_count; i++)
            memory_[initial->ram[i].address] = initial->ram[i].value;

        t_state_count_ = 0;
        prev_iorq_active_ = false;
    }

    // Load CPU registers from test initial state
    void load_state(const z80_cpu_state_t* s) {
        // Full re-init to clear internal state machines
        pins_ = cpu.init();

        cpu.set_pc(s->pc);
        cpu.set_sp(s->sp);
        cpu.set_af(static_cast<uint16_t>(s->a) << 8 | s->f);
        cpu.set_bc(static_cast<uint16_t>(s->b) << 8 | s->c);
        cpu.set_de(static_cast<uint16_t>(s->d) << 8 | s->e);
        cpu.set_hl_direct(static_cast<uint16_t>(s->h) << 8 | s->l);
        cpu.set_ix(s->ix);
        cpu.set_iy(s->iy);
        cpu.set_i(s->i);
        cpu.set_r(s->r);
        cpu.set_im(s->im);
        cpu.set_iff1(s->iff1);
        cpu.set_iff2(s->iff2);
        cpu.set_wz(s->wz);
        cpu.set_af_prime(s->af_);
        cpu.set_bc_prime(s->bc_);
        cpu.set_de_prime(s->de_);
        cpu.set_hl_prime(s->hl_);
        cpu.set_ei_pending(s->ei);
        cpu.set_q(s->q);

        // Ensure interrupt lines are inactive (high) for test
        pins_ = CPU::default_bus_state();
    }

    // === Execution ===

    // Service the bus between T-states:
    // Read data bus on memory read, write data to memory on write,
    // handle I/O reads/writes
    bus_state_t service_bus(bus_state_t pins) {
        bool rd   = BUS_GET_BIT(pins, BUS_RW_BIT);      // RW high = read
        bool wr   = !rd;                                  // RW low  = write
        bool mreq = !BUS_GET_BIT(pins, Z80_MREQ_BIT);   // Active-low
        bool iorq = !BUS_GET_BIT(pins, Z80_IORQ_BIT);   // Active-low
        bool m1   = !BUS_GET_BIT(pins, Z80_M1_BIT);      // Active-low

        uint16_t addr = BUS_GET_ADDR(pins);

        // Record bus cycle
        recorded_bus_cycle_t rec;
        rec.address = addr;
        rec.data = BUS_GET_DATA(pins);
        rec.data_valid = true;
        rec.rd = rd && (mreq || iorq);
        rec.wr = wr && (mreq || iorq);
        rec.mreq = mreq;
        rec.iorq = iorq;
        actual_bus_cycles_.push_back(rec);

        // Memory read (MREQ + RD active)
        if (mreq && rd && !iorq) {
            BUS_SET_DATA(pins, memory_[addr]);
        }
        // Memory write (MREQ + WR active)
        else if (mreq && wr && !iorq) {
            memory_[addr] = BUS_GET_DATA(pins);
        }
        // I/O read (IORQ + RD active, not M1 — M1+IORQ = interrupt ack)
        else if (iorq && rd && !m1) {
            uint8_t io_data = io_ports_[addr];
            BUS_SET_DATA(pins, io_data);
            // Only record port access on the first T-state where IORQ becomes active
            if (!prev_iorq_active_) {
                z80_port_access_t pa;
                pa.address = addr;
                pa.data = io_data;
                pa.is_write = false;
                actual_ports_.push_back(pa);
            }
        }
        // I/O write (IORQ + WR active)
        else if (iorq && wr && !m1) {
            uint8_t io_data = BUS_GET_DATA(pins);
            io_ports_[addr] = io_data;
            // Only record port access on the first T-state where IORQ becomes active
            if (!prev_iorq_active_) {
                z80_port_access_t pa;
                pa.address = addr;
                pa.data = io_data;
                pa.is_write = true;
                actual_ports_.push_back(pa);
            }
        }

        prev_iorq_active_ = iorq && !m1;
        return pins;
    }

    // Execute one instruction. Returns false on timeout.
    bool step() {
        constexpr uint32_t MAX_T_STATES = 100; // Safety limit for longest Z80 instruction
        uint32_t t = 0;

        // First tick to start the instruction (enters M1 fetch T1)
        pins_ = cpu.tick(pins_);
        pins_ = service_bus(pins_);
        t_state_count_++;
        t++;

        // Continue ticking until instruction completes (opdone returns true at
        // the start of the next M1 fetch cycle with step_==0)
        while (t < MAX_T_STATES) {
            pins_ = cpu.tick(pins_);
            pins_ = service_bus(pins_);
            t_state_count_++;
            t++;

            if (cpu.opdone()) break;
        }

        return t < MAX_T_STATES;
    }

    // === Accessors ===
    uint32_t get_t_state_count() const { return t_state_count_; }
    const std::vector<z80_port_access_t>& get_actual_ports() const { return actual_ports_; }
    const std::vector<recorded_bus_cycle_t>& get_actual_bus_cycles() const { return actual_bus_cycles_; }

    // Get current CPU state for comparison
    void read_state(z80_cpu_state_t* s) const {
        s->pc = cpu.pc();
        s->sp = cpu.sp();
        uint16_t af = cpu.af();
        s->a = static_cast<uint8_t>(af >> 8);
        s->f = static_cast<uint8_t>(af & 0xFF);
        uint16_t bc = cpu.bc();
        s->b = static_cast<uint8_t>(bc >> 8);
        s->c = static_cast<uint8_t>(bc & 0xFF);
        uint16_t de = cpu.de();
        s->d = static_cast<uint8_t>(de >> 8);
        s->e = static_cast<uint8_t>(de & 0xFF);
        uint16_t hl = cpu.hl();
        s->h = static_cast<uint8_t>(hl >> 8);
        s->l = static_cast<uint8_t>(hl & 0xFF);
        s->i  = cpu.i();
        s->r  = cpu.r();
        s->ix = cpu.ix();
        s->iy = cpu.iy();
        s->wz = cpu.wz();
        s->af_ = cpu.af_prime();
        s->bc_ = cpu.bc_prime();
        s->de_ = cpu.de_prime();
        s->hl_ = cpu.hl_prime();
        s->im   = cpu.im();
        s->iff1 = cpu.iff1();
        s->iff2 = cpu.iff2();
        // ei, p, q are internal tracking — not directly readable
        s->ei = false;
        s->p = false;
        s->q = cpu.q();
        s->ram_count = 0;
    }

    // Set up I/O port values expected by an IN instruction
    void setup_io_for_test(const z80_test_t* test) {
        if (!test->has_ports) return;
        for (uint8_t i = 0; i < test->port_count; i++) {
            if (!test->ports[i].is_write) {
                // Pre-load port value for IN instructions
                io_ports_[test->ports[i].address] = test->ports[i].data;
            }
        }
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
class Z80TestWorkerPool {
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
    Z80TestWorkerPool(size_t n, ThreadSafeOutput& out, ThreadSafeTestResults& res,
                      std::atomic<bool>& failed)
        : output_(out), results_(res), test_failed_(failed)
    {
        for (size_t i = 0; i < n; i++)
            workers_.emplace_back(&Z80TestWorkerPool::worker, this);
    }

    ~Z80TestWorkerPool() {
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
        Z80TestHarness<CPU> harness;
        z80_test_t* prev_test = nullptr;

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

            z80_test_t test;
            if (!z80_parse_test(item.test_json.c_str(), &test)) {
                std::ostringstream os;
                os << "ERROR: Failed to parse test: " << item.filepath << "\n";
                output_.add(os.str());
                results_.total_tests++;
                results_.failed_tests++;
                continue;
            }

            bool result = run_test(&test, harness, prev_test);
            if (!prev_test)
                prev_test = new z80_test_t(test);
            else
                *prev_test = test;

            if (!result && g_stop_on_failure)
                test_failed_ = true;
        }

        delete prev_test;
    }

    bool run_test(const z80_test_t* test, Z80TestHarness<CPU>& harness,
                  const z80_test_t* prev) {
        results_.total_tests++;

        std::ostringstream debug;
        bool suppress_verbose = false;
        if (g_verbose) {
            size_t cnt = g_verbose_count.fetch_add(1);
            if (cnt >= MAX_VERBOSE_OUTPUTS)
                suppress_verbose = true;
            else
                debug << "Running test: " << test->name << "\n";
        }

        // Setup
        const z80_cpu_state_t* prev_final = prev ? &prev->final_state : nullptr;
        harness.setup_for_test(&test->initial, prev_final);
        harness.load_state(&test->initial);
        harness.setup_io_for_test(test);

        // Determine opcode for statistics
        uint8_t opcode = harness.get_memory(test->initial.pc);

        // Execute
        if (!harness.step()) {
            if (!g_quiet) {
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
        bool port_ok = true;

        z80_cpu_state_t actual;
        harness.read_state(&actual);
        const z80_cpu_state_t& expected = test->final_state;

        // Register comparison macro
        #define CHECK_REG(reg_name, actual_val, expected_val) \
            if ((actual_val) != (expected_val)) { \
                if (!g_quiet) { \
                    debug << "FAIL " << test->name << ": " << reg_name << " - expected 0x" \
                          << std::hex << static_cast<int>(expected_val) \
                          << ", got 0x" << static_cast<int>(actual_val) << std::dec << "\n"; \
                } \
                state_ok = false; \
            }

        CHECK_REG("PC", actual.pc, expected.pc);
        CHECK_REG("SP", actual.sp, expected.sp);
        CHECK_REG("A", actual.a, expected.a);
        CHECK_REG("F", actual.f, expected.f);
        CHECK_REG("B", actual.b, expected.b);
        CHECK_REG("C", actual.c, expected.c);
        CHECK_REG("D", actual.d, expected.d);
        CHECK_REG("E", actual.e, expected.e);
        CHECK_REG("H", actual.h, expected.h);
        CHECK_REG("L", actual.l, expected.l);
        CHECK_REG("I", actual.i, expected.i);
        CHECK_REG("R", actual.r, expected.r);
        CHECK_REG("IX", actual.ix, expected.ix);
        CHECK_REG("IY", actual.iy, expected.iy);
        CHECK_REG("WZ", actual.wz, expected.wz);
        CHECK_REG("AF'", actual.af_, expected.af_);
        CHECK_REG("BC'", actual.bc_, expected.bc_);
        CHECK_REG("DE'", actual.de_, expected.de_);
        CHECK_REG("HL'", actual.hl_, expected.hl_);
        CHECK_REG("IM", actual.im, expected.im);
        CHECK_REG("IFF1", actual.iff1 ? 1 : 0, expected.iff1 ? 1 : 0);
        CHECK_REG("IFF2", actual.iff2 ? 1 : 0, expected.iff2 ? 1 : 0);

        #undef CHECK_REG

        // Memory comparison
        for (uint8_t i = 0; i < expected.ram_count; i++) {
            uint8_t actual_val = harness.get_memory(expected.ram[i].address);
            if (actual_val != expected.ram[i].value) {
                if (!g_quiet) {
                    debug << "FAIL " << test->name << ": Memory[0x" << std::hex
                          << expected.ram[i].address << "] - expected 0x"
                          << static_cast<int>(expected.ram[i].value)
                          << ", got 0x" << static_cast<int>(actual_val) << std::dec << "\n";
                }
                state_ok = false;
            }
        }

        // T-state count comparison (cycle count = number of bus cycles recorded matches test cycles)
        if (test->has_cycles) {
            uint16_t expected_cycles = test->cycle_count;
            auto& actual_cycles = harness.get_actual_bus_cycles();
            if (actual_cycles.size() != expected_cycles) {
                if (!g_quiet) {
                    debug << "FAIL " << test->name << ": Cycle count - expected "
                          << expected_cycles << ", got " << actual_cycles.size() << "\n";
                }
                cycle_ok = false;
            }
        }

        // I/O port comparison
        if (test->has_ports) {
            auto& actual_pa = harness.get_actual_ports();
            if (actual_pa.size() != test->port_count) {
                if (!g_quiet) {
                    debug << "FAIL " << test->name << ": Port transaction count - expected "
                          << static_cast<int>(test->port_count) << ", got "
                          << actual_pa.size() << "\n";
                }
                port_ok = false;
            } else {
                for (uint8_t i = 0; i < test->port_count; i++) {
                    if (actual_pa[i].address != test->ports[i].address ||
                        actual_pa[i].data != test->ports[i].data ||
                        actual_pa[i].is_write != test->ports[i].is_write) {
                        if (!g_quiet) {
                            debug << "FAIL " << test->name << ": Port[" << i
                                  << "] addr=0x" << std::hex << actual_pa[i].address
                                  << " data=0x" << static_cast<int>(actual_pa[i].data)
                                  << (actual_pa[i].is_write ? "W" : "R")
                                  << " expected addr=0x" << test->ports[i].address
                                  << " data=0x" << static_cast<int>(test->ports[i].data)
                                  << (test->ports[i].is_write ? "W" : "R")
                                  << std::dec << "\n";
                        }
                        port_ok = false;
                        break;
                    }
                }
            }
        }

        // Record result
        if (state_ok && cycle_ok && port_ok) {
            results_.passed_tests++;
            results_.record_opcode_result(opcode, true);
            if (g_verbose && !suppress_verbose) {
                debug << "PASS " << test->name << " (opcode 0x" << std::hex
                      << static_cast<int>(opcode) << ")\n" << std::dec;
                output_.add(debug.str());
            }
            return true;
        } else {
            results_.failed_tests++;
            if (!state_ok) results_.state_mismatches++;
            if (!cycle_ok) results_.cycle_mismatches++;
            if (!port_ok) results_.port_mismatches++;
            results_.record_opcode_result(opcode, false);
            if (!g_quiet) {
                debug << "FAIL " << test->name << ": ";
                if (!state_ok) debug << "State ";
                if (!cycle_ok) debug << "Cycle ";
                if (!port_ok)  debug << "Port ";
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

static void collect_tests_from_file(const std::string& filepath, std::vector<TestItem>& tests) {
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
                    if (entry.is_regular_file(ec) && !ec && entry.path().extension() == ".json") {
                        if (!opcode_filter.empty()) {
                            std::string stem = entry.path().stem().string();
                            std::transform(stem.begin(), stem.end(), stem.begin(),
                                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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
    std::cout << "Z80 SingleStepTests Runner\n";
    std::cout << "Usage: " << prog << " [options] <test_file_or_directory>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -v, --verbose     Verbose output\n";
    std::cout << "  -q, --quiet       Quiet mode (summary only)\n";
    std::cout << "  -c, --continue    Continue after failures\n";
    std::cout << "  -s, --stop-first  Stop on first failure (default)\n";
    std::cout << "  -j, --jobs N      Worker threads (default: cores-1)\n";
    std::cout << "  -o, --opcode HH   Filter to single opcode (e.g. db or 0xDB)\n";
    std::cout << "  -h, --help        This message\n\n";
    std::cout << "Test Suite: https://github.com/SingleStepTests/z80 (v1/)\n";
    std::cout << "Place the v1/ directory somewhere accessible and pass its path.\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " path/to/z80/v1/\n";
    std::cout << "  " << prog << " -o db -v path/to/z80/v1/\n";
    std::cout << "  " << prog << " -j 8 -c path/to/z80/v1/\n";
}

static void print_results(const TestResults& r, std::chrono::milliseconds dur,
                           size_t workers, size_t collected) {
    std::cout << "\n=== Z80 PROCESSOR TESTS RESULTS ===\n";
    std::cout << "CPU: Zilog Z80 (NMOS)\n";
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
        std::cout << "Performance: " << std::fixed << std::setprecision(1) << tps << " tests/second\n";
    }

    if (r.failed_tests > 0) {
        std::cout << "\nFailure breakdown:\n";
        std::cout << "  State mismatches: " << r.state_mismatches << "\n";
        std::cout << "  Cycle mismatches: " << r.cycle_mismatches << "\n";
        std::cout << "  Port mismatches: " << r.port_mismatches << "\n";

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
        std::cout << "\nALL TESTS PASSED - Z80 implementation is hardware-accurate!\n";
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
    std::cout << "=== Z80 SingleStepTests Runner ===\n";
    std::cout << "CPU: Zilog Z80 (NMOS)\n";
    std::cout << "Worker threads: " << num_workers << "\n";
    if (!opcode_filter.empty())
        std::cout << "Opcode filter: 0x" << opcode_filter << "\n";
    std::cout << "Verbose: " << (g_verbose ? "yes" : "no") << "\n";
    std::cout << "Quiet: " << (g_quiet ? "yes" : "no") << "\n";
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
    Z80TestWorkerPool<CPU> pool(effective, output, thread_results, g_test_failed);

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
            g_verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            g_quiet = true;
        } else if (arg == "-c" || arg == "--continue") {
            g_stop_on_failure = false;
        } else if (arg == "-s" || arg == "--stop-first") {
            g_stop_on_failure = true;
        } else if (arg == "-j" || arg == "--jobs") {
            if (i + 1 < argc) num_workers = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "-o" || arg == "--opcode") {
            if (i + 1 < argc) {
                opcode_filter = argv[++i];
                if (opcode_filter.substr(0, 2) == "0x" || opcode_filter.substr(0, 2) == "0X")
                    opcode_filter = opcode_filter.substr(2);
                std::transform(opcode_filter.begin(), opcode_filter.end(), opcode_filter.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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

    // Currently targets the standard NMOS Z80.
    // Future: add -p flag for Z80A/Z80B/U880 variants.
    return run_all_tests<ZilogZ80>(test_paths, opcode_filter, num_workers);
}
