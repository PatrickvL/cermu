/*
 * m680x0_processor_tests_runner.cpp — M680x0 SingleStepTests runner
 *
 * Runs the JSON-based 68000 single-step tests from:
 *   https://github.com/SingleStepTests/m68000
 *
 * Follows the ProcessorTestHarness pattern established by the fam65xx
 * and Z80 processor tests runners: thread-local memory, multi-threaded
 * worker pool, bus-cycle tracing, per-opcode statistics.
 *
 * M680x0-specific adaptations:
 *   - 16-bit data bus with UDS/LDS data strobes
 *   - Big-endian byte ordering
 *   - 24-bit address space (MC68000/10), 32-bit (MC68020)
 *   - D0-D7 (32-bit) + A0-A7 (32-bit) + USP/SSP
 *   - 16-bit SR (supervisor byte + CCR)
 *   - Prefetch pipeline (IRC, IR, IRD)
 *   - Function codes (FC0-FC2)
 *   - Bus cycle: AS + UDS/LDS + DTACK handshake
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

#include <archive.h>
#include <archive_entry.h>

// Low-level JSON utilities (shared with fam65xx/Z80 runners)
#include "json_parser.hpp"

#ifndef CERMU_IMPL
    #define CERMU_IMPL
#endif

// M680x0 CPU variant headers
#include "../src/chip/cpu/m680x0/mc68000.hpp"
#include "../src/chip/cpu/m680x0/mc68010.hpp"
#include "../src/chip/cpu/m680x0/mc68020.hpp"

using namespace m680x0;

namespace fs = std::filesystem;

// ============================================================================
// Thread-local memory for CPU testing
// ============================================================================
// 64KB flat memory for fast-path addresses (0x0000-0xFFFF).
// Higher addresses use the per-harness extended_memory map.

thread_local uint8_t test_memory[65536];

// ============================================================================
// Global flags
// ============================================================================

static bool s_verbose = false;
static bool s_quiet = false;
static bool s_stop_on_failure = true;
static std::atomic<bool> s_test_failed{false};
static std::atomic<size_t> s_verbose_count{0};
static constexpr size_t MAX_VERBOSE_OUTPUTS = 1000;

// ============================================================================
// M68K-specific test data structures
// ============================================================================

// M68K CPU state for initial/final comparison
struct m68k_cpu_state_t {
    uint32_t d[8];          // D0-D7
    uint32_t a[7];          // A0-A6 (A7 is SSP or USP depending on mode)
    uint32_t usp;
    uint32_t ssp;
    uint16_t sr;
    uint32_t pc;
    uint16_t prefetch[2];   // [0] = IRC, [1] = IR
    bool     has_prefetch;

    // RAM entries: [address, byte_value]
    struct { uint32_t address; uint8_t value; } ram[256];
    uint16_t ram_count;
};

// M68K bus transaction entry (from test data)
// Format: ["r"/"w", clocks, fc, address, ".w"/".b", value]
//     or: ["n", clocks]  (idle)
struct m68k_transaction_t {
    char     type;          // 'n' = idle, 'r' = read, 'w' = write
    uint8_t  clocks;        // number of clock cycles for this bus cycle
    uint8_t  fc;            // function code (FC0-FC2)
    uint32_t address;
    uint16_t value;         // 16-bit for word accesses, 8-bit for byte
    bool     is_word;       // true = .w, false = .b
    bool     has_addr;      // false for idle cycles
};

// Recorded bus cycle (actual, for comparison)
struct m68k_recorded_cycle_t {
    uint32_t address;
    uint8_t  data;
    bool     is_read;       // true = read, false = write
    bool     is_active;     // false for idle cycles (no AS asserted)
};

// Complete M68K test case
struct m68k_test_t {
    char name[128];
    m68k_cpu_state_t initial;
    m68k_cpu_state_t final_state;

    // Expected bus transactions
    m68k_transaction_t transactions[128];
    uint16_t transaction_count;
    bool has_transactions;

    // Expected total length (clocks)
    uint32_t length;
    bool has_length;
};

// ============================================================================
// M68K JSON parser
// ============================================================================

// Parse a 32-bit unsigned value from JSON (handles large values)
static uint32_t json_parse_u32(const char* json, const char* key) {
    const char* val = json_find_key(json, key);
    if (!val) return 0;
    val = json_skip_whitespace(val);
    return static_cast<uint32_t>(strtoul(val, nullptr, 0));
}

// Parse an M68K CPU state object from JSON
static bool m68k_parse_state(const char* json, const char* state_name, m68k_cpu_state_t* state) {
    const char* obj = json_find_key(json, state_name);
    if (!obj || *obj != '{') return false;

    // Data registers D0-D7
    const char* reg_names_d[] = {"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7"};
    for (int i = 0; i < 8; i++)
        state->d[i] = json_parse_u32(obj, reg_names_d[i]);

    // Address registers A0-A6
    const char* reg_names_a[] = {"a0", "a1", "a2", "a3", "a4", "a5", "a6"};
    for (int i = 0; i < 7; i++)
        state->a[i] = json_parse_u32(obj, reg_names_a[i]);

    state->usp = json_parse_u32(obj, "usp");
    state->ssp = json_parse_u32(obj, "ssp");
    state->sr  = static_cast<uint16_t>(json_parse_number(obj, "sr"));
    state->pc  = json_parse_u32(obj, "pc");

    // Parse prefetch array: [IRC, IR]
    state->has_prefetch = false;
    state->prefetch[0] = 0;
    state->prefetch[1] = 0;
    const char* pf = json_find_key(obj, "prefetch");
    if (pf && *pf == '[') {
        state->has_prefetch = true;
        const char* pos = pf + 1;
        pos = json_skip_whitespace(pos);
        state->prefetch[0] = static_cast<uint16_t>(strtol(pos, const_cast<char**>(&pos), 0));
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;
        pos = json_skip_whitespace(pos);
        state->prefetch[1] = static_cast<uint16_t>(strtol(pos, const_cast<char**>(&pos), 0));
    }

    // Parse RAM array: [[addr, byte], ...]
    state->ram_count = 0;
    const char* ram_arr = json_find_key(obj, "ram");
    if (ram_arr && *ram_arr == '[') {
        const char* pos = ram_arr + 1;
        while (*pos && state->ram_count < 256) {
            pos = json_skip_whitespace(pos);
            if (*pos == ']') break;
            if (*pos != '[') break;
            pos++;
            pos = json_skip_whitespace(pos);
            state->ram[state->ram_count].address =
                static_cast<uint32_t>(strtoul(pos, const_cast<char**>(&pos), 0));
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

// Parse transactions array (optional bus-cycle trace)
// Format: [["r", 4, 6, 3076, ".w", 1657], ["n", 2], ...]
static bool m68k_parse_transactions(const char* json, m68k_test_t* test) {
    const char* arr = json_find_key(json, "transactions");
    if (!arr || *arr != '[') {
        test->has_transactions = false;
        test->transaction_count = 0;
        return true;
    }
    test->has_transactions = true;
    test->transaction_count = 0;
    const char* pos = arr + 1;

    while (*pos && test->transaction_count < 128) {
        pos = json_skip_whitespace(pos);
        if (*pos == ']') break;
        if (*pos != '[') { pos++; continue; }
        pos++;  // skip opening '['

        m68k_transaction_t& txn = test->transactions[test->transaction_count];
        txn.type = 'n';
        txn.clocks = 0;
        txn.fc = 0;
        txn.address = 0;
        txn.value = 0;
        txn.is_word = true;
        txn.has_addr = false;

        // Element 0: type string ("r", "w", "n")
        pos = json_skip_whitespace(pos);
        if (*pos == '"') {
            pos++;
            txn.type = *pos;
            while (*pos && *pos != '"') pos++;
            if (*pos == '"') pos++;
        }
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;

        // Element 1: clocks
        pos = json_skip_whitespace(pos);
        txn.clocks = static_cast<uint8_t>(strtol(pos, const_cast<char**>(&pos), 0));
        pos = json_skip_whitespace(pos);

        // For idle cycles ("n"), the array has only 2 elements
        if (txn.type == 'n') {
            // Skip to end of array
            while (*pos && *pos != ']') pos++;
            if (*pos == ']') pos++;
            test->transaction_count++;
            pos = json_skip_whitespace(pos);
            if (*pos == ',') pos++;
            continue;
        }

        if (*pos == ',') pos++;

        // Element 2: function code
        pos = json_skip_whitespace(pos);
        txn.fc = static_cast<uint8_t>(strtol(pos, const_cast<char**>(&pos), 0));
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;

        // Element 3: address
        pos = json_skip_whitespace(pos);
        txn.address = static_cast<uint32_t>(strtoul(pos, const_cast<char**>(&pos), 0));
        txn.has_addr = true;
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;

        // Element 4: size string (".w" or ".b")
        pos = json_skip_whitespace(pos);
        if (*pos == '"') {
            pos++;
            if (pos[0] == '.' && pos[1] == 'b') txn.is_word = false;
            while (*pos && *pos != '"') pos++;
            if (*pos == '"') pos++;
        }
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;

        // Element 5: value
        pos = json_skip_whitespace(pos);
        txn.value = static_cast<uint16_t>(strtoul(pos, const_cast<char**>(&pos), 0));
        pos = json_skip_whitespace(pos);

        // Skip to end of array
        while (*pos && *pos != ']') pos++;
        if (*pos == ']') pos++;
        test->transaction_count++;
        pos = json_skip_whitespace(pos);
        if (*pos == ',') pos++;
    }
    return true;
}

// Parse a complete M68K test case from JSON
static bool m68k_parse_test(const char* json, m68k_test_t* test) {
    std::memset(test, 0, sizeof(*test));
    json_parse_string(json, "name", test->name, sizeof(test->name));
    if (!m68k_parse_state(json, "initial", &test->initial)) return false;
    if (!m68k_parse_state(json, "final", &test->final_state)) return false;
    m68k_parse_transactions(json, test);

    // Parse length (total clocks)
    int len = json_parse_number(json, "length");
    test->has_length = (len > 0);
    test->length = static_cast<uint32_t>(len);

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
    // 68000 has 16-bit opcodes → 65536 possible values
    // Track by upper byte (group + first nybble) for manageable stats
    uint32_t opcode_failures[256] = {};
    uint32_t opcode_totals[256] = {};
};

struct ThreadSafeTestResults {
    std::atomic<uint32_t> total_tests{0};
    std::atomic<uint32_t> passed_tests{0};
    std::atomic<uint32_t> failed_tests{0};
    std::atomic<uint32_t> state_mismatches{0};
    std::atomic<uint32_t> cycle_mismatches{0};

    std::mutex opcode_mutex;
    uint32_t opcode_failures[256] = {};
    uint32_t opcode_totals[256] = {};

    void record_opcode_result(uint8_t opcode_hi, bool success) {
        std::lock_guard<std::mutex> lock(opcode_mutex);
        opcode_totals[opcode_hi]++;
        if (!success) opcode_failures[opcode_hi]++;
    }

    void merge_into(TestResults& r) {
        r.total_tests = total_tests.load();
        r.passed_tests = passed_tests.load();
        r.failed_tests = failed_tests.load();
        r.state_mismatches = state_mismatches.load();
        r.cycle_mismatches = cycle_mismatches.load();
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
// M68K Test Harness — templated on the M680x0 variant
// ============================================================================

template <typename CPU>
class M68KTestHarness {
public:
    CPU cpu;

private:
    uint8_t* memory_;
    std::unordered_map<uint32_t, uint8_t> extended_memory_;  // Addresses > 0xFFFF
    uint32_t cycle_count_;
    bus_state_t pins_;

    // Recorded bus cycles
    std::vector<m68k_recorded_cycle_t> actual_bus_cycles_;

    // Helper: full-address memory read
    uint8_t mem_read(uint32_t addr) const {
        addr &= CPU::address_mask();
        if (addr < 65536) return memory_[addr];
        auto it = extended_memory_.find(addr);
        return (it != extended_memory_.end()) ? it->second : 0;
    }

    // Helper: full-address memory write
    void mem_write(uint32_t addr, uint8_t data) {
        addr &= CPU::address_mask();
        if (addr < 65536) memory_[addr] = data;
        else extended_memory_[addr] = data;
    }

    // ── Callback trampolines for CPU memory access ──────────────
    static uint16_t cb_read_word(void* ctx, uint32_t addr) {
        auto* self = static_cast<M68KTestHarness*>(ctx);
        addr &= CPU::address_mask();
        uint8_t hi = self->mem_read(addr);
        uint8_t lo = self->mem_read(addr + 1);
        return (static_cast<uint16_t>(hi) << 8) | lo;
    }

    static void cb_write_word(void* ctx, uint32_t addr, uint16_t data) {
        auto* self = static_cast<M68KTestHarness*>(ctx);
        addr &= CPU::address_mask();
        self->mem_write(addr, (data >> 8) & 0xFF);
        self->mem_write(addr + 1, data & 0xFF);
    }

public:
    M68KTestHarness()
        : memory_(test_memory), cycle_count_(0), pins_(CPU::default_bus_state()) {
        std::fill(memory_, memory_ + 65536, static_cast<uint8_t>(0));
        extended_memory_.clear();
        cpu.init();
        cpu.set_memory_callbacks(&cb_read_word, &cb_write_word, this);
        actual_bus_cycles_.reserve(64);
    }

    // === Memory access ===
    void set_memory(uint32_t addr, uint8_t data) { mem_write(addr, data); }
    uint8_t get_memory(uint32_t addr) const { return mem_read(addr); }
    void clear_memory(uint32_t addr) { mem_write(addr, 0); }

    // === Setup ===
    void setup_for_test(const m68k_cpu_state_t* initial, const m68k_cpu_state_t* prev_final) {
        actual_bus_cycles_.clear();
        actual_bus_cycles_.reserve(64);

        // Selective memory clearing
        if (prev_final) {
            for (uint16_t i = 0; i < prev_final->ram_count; i++) {
                uint32_t addr = prev_final->ram[i].address & CPU::address_mask();
                if (addr < 65536) memory_[addr] = 0;
                else extended_memory_.erase(addr);
            }
        } else {
            std::fill(memory_, memory_ + 65536, static_cast<uint8_t>(0));
            extended_memory_.clear();
        }

        // Load initial RAM
        for (uint16_t i = 0; i < initial->ram_count; i++)
            mem_write(initial->ram[i].address, initial->ram[i].value);

        cycle_count_ = 0;
    }

    // Load CPU registers from test initial state
    void load_state(const m68k_cpu_state_t* s) {
        // Full re-init to clear internal state machines
        pins_ = cpu.init();
        cpu.set_memory_callbacks(&cb_read_word, &cb_write_word, this);

        // Data registers
        for (int i = 0; i < 8; i++)
            cpu.set_reg_d(i, s->d[i]);

        // Address registers A0-A6
        for (int i = 0; i < 7; i++)
            cpu.set_reg_a(i, s->a[i]);

        // Stack pointers
        cpu.set(USP, s->usp);
        cpu.set(SSP, s->ssp);

        // A7 depends on supervisor mode: if S bit set, A7 = SSP; else A7 = USP
        if (s->sr & m680x0::SRBits::S) {
            cpu.set_reg_a(7, s->ssp);
        } else {
            cpu.set_reg_a(7, s->usp);
        }

        // SR (includes CCR + supervisor byte)
        cpu.set(SR, s->sr);

        // Program counter — the test's 'pc' is the formal PC (instruction address).
        // The 68000's internal PC is 4 bytes ahead (two prefetched words).
        cpu.set(PC, s->pc + 4);

        // Prefetch pipeline:
        //   prefetch[0] = "fetched earlier" = IR = IRD (current instruction)
        //   prefetch[1] = "fetched later"   = IRC (next prefetched word)
        if (s->has_prefetch) {
            cpu.set(IR, s->prefetch[0]);
            cpu.set(IRD, s->prefetch[0]);   // IRD = IR at instruction boundary
            cpu.set(IRC, s->prefetch[1]);
        }

        // Set CPU state to "ready to decode" (skip reset sequence)
        cpu.prepare_for_test();

        // Ensure interrupt lines are inactive (IPL = 0 = no interrupt)
        pins_ = CPU::default_bus_state();
        // RESET deasserted (active-low, high = inactive)
        BUS_SET_BIT(pins_, M68K_RESET_BIT);
        // Set IPL lines high (inactive — active-low, inverted: all high = priority 0)
        BUS_SET_BIT(pins_, M68K_IPL0_BIT);
        BUS_SET_BIT(pins_, M68K_IPL1_BIT);
        BUS_SET_BIT(pins_, M68K_IPL2_BIT);
        // DTACK not asserted initially
        BUS_SET_BIT(pins_, M68K_DTACK_BIT);
    }

    // === Execution ===

    // Service the bus between clock cycles:
    // Respond to memory reads/writes based on bus control signals
    bus_state_t service_bus(bus_state_t pins) {
        bool as_active = !BUS_GET_BIT(pins, M68K_AS_BIT);   // Active-low
        bool rw_read   = BUS_GET_BIT(pins, BUS_RW_BIT);     // High = read
        bool uds_active = !BUS_GET_BIT(pins, M68K_UDS_BIT); // Active-low
        bool lds_active = !BUS_GET_BIT(pins, M68K_LDS_BIT); // Active-low

        if (as_active && (uds_active || lds_active)) {
            uint32_t addr = BUS_GET_ADDR(pins);

            if (rw_read) {
                if (uds_active && lds_active) {
                    // Word read — put 16-bit word on bus (high in DATA, low in BANK)
                    uint8_t hi = mem_read(addr);
                    uint8_t lo = mem_read(addr + 1);
                    uint16_t word = (static_cast<uint16_t>(hi) << 8) | lo;
                    M68K_SET_DATA_WORD(pins, word);
                } else if (uds_active) {
                    // Byte read from even address (D15-D8)
                    BUS_SET_DATA(pins, mem_read(addr));
                } else {
                    // Byte read from odd address (D7-D0)
                    BUS_SET_DATA(pins, mem_read(addr));
                }
                // Assert DTACK (active-low — clear the bit)
                BUS_CLR_BIT(pins, M68K_DTACK_BIT);
            } else {
                if (uds_active && lds_active) {
                    // Word write — read 16-bit word from bus
                    uint16_t word = M68K_GET_DATA_WORD(pins);
                    mem_write(addr, (word >> 8) & 0xFF);
                    mem_write(addr + 1, word & 0xFF);
                } else {
                    // Byte write
                    mem_write(addr, BUS_GET_DATA(pins));
                }
                // Assert DTACK
                BUS_CLR_BIT(pins, M68K_DTACK_BIT);
            }
        } else {
            // No bus cycle active — deassert DTACK
            BUS_SET_BIT(pins, M68K_DTACK_BIT);
        }

        return pins;
    }

    // Execute one instruction. Returns false on timeout.
    bool step() {
        constexpr uint32_t MAX_CLOCKS = 500;  // Safety limit for longest M68K instruction
        uint32_t t = 0;

        // First tick to start the instruction (enters decode)
        pins_ = cpu.tick(pins_);
        pins_ = service_bus(pins_);
        cycle_count_++;
        t++;

        // Continue ticking until instruction completes (opdone returns true
        // at the start of the next decode cycle)
        while (t < MAX_CLOCKS) {
            pins_ = cpu.tick(pins_);
            pins_ = service_bus(pins_);
            cycle_count_++;
            t++;

            if (cpu.opdone()) break;
        }

        return t < MAX_CLOCKS;
    }

    // === Accessors ===
    uint32_t get_cycle_count() const { return cycle_count_; }
    const std::vector<m68k_recorded_cycle_t>& get_actual_bus_cycles() const {
        return actual_bus_cycles_;
    }

    // Get current CPU state for comparison
    void read_state(m68k_cpu_state_t* s) const {
        for (int i = 0; i < 8; i++)
            s->d[i] = cpu.reg_d(i);
        for (int i = 0; i < 7; i++)
            s->a[i] = cpu.reg_a(i);

        s->sr  = cpu.get(SR);
        s->pc  = cpu.get(PC) - 4;   // Convert internal PC back to formal PC
        s->usp = cpu.get(USP);
        s->ssp = cpu.get(SSP);

        // prefetch[0] = IR (fetched earlier), prefetch[1] = IRC (fetched later)
        s->prefetch[0] = cpu.get(IR);
        s->prefetch[1] = cpu.get(IRC);
        s->has_prefetch = true;

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
class M68KTestWorkerPool {
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
    M68KTestWorkerPool(size_t n, ThreadSafeOutput& out, ThreadSafeTestResults& res,
                       std::atomic<bool>& failed)
        : output_(out), results_(res), test_failed_(failed)
    {
        for (size_t i = 0; i < n; i++)
            workers_.emplace_back(&M68KTestWorkerPool::worker, this);
    }

    ~M68KTestWorkerPool() {
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
            if (s_stop_on_failure && test_failed_.load()) {
                { std::lock_guard<std::mutex> lk(queue_mtx_); shutdown_ = true; }
                queue_cv_.notify_all();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    void worker() {
        M68KTestHarness<CPU> harness;
        m68k_test_t* prev_test = nullptr;

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

            if (s_stop_on_failure && test_failed_.load()) break;

            m68k_test_t test;
            if (!m68k_parse_test(item.test_json.c_str(), &test)) {
                std::ostringstream os;
                os << "ERROR: Failed to parse test: " << item.filepath << "\n";
                output_.add(os.str());
                results_.total_tests++;
                results_.failed_tests++;
                continue;
            }

            bool result = run_test(&test, harness, prev_test);
            if (!prev_test)
                prev_test = new m68k_test_t(test);
            else
                *prev_test = test;

            if (!result && s_stop_on_failure)
                test_failed_ = true;
        }

        delete prev_test;
    }

    bool run_test(const m68k_test_t* test, M68KTestHarness<CPU>& harness,
                  const m68k_test_t* prev) {
        results_.total_tests++;

        std::ostringstream debug;
        bool suppress_verbose = false;
        if (s_verbose) {
            size_t cnt = s_verbose_count.fetch_add(1);
            if (cnt >= MAX_VERBOSE_OUTPUTS)
                suppress_verbose = true;
            else
                debug << "Running test: " << test->name << "\n";
        }

        // Setup
        const m68k_cpu_state_t* prev_final = prev ? &prev->final_state : nullptr;
        harness.setup_for_test(&test->initial, prev_final);
        harness.load_state(&test->initial);

        // Determine opcode (upper byte of first instruction word)
        // The instruction word is in prefetch[0] (= IR/IRD at instruction boundary)
        uint8_t opcode_hi = (test->initial.prefetch[0] >> 8) & 0xFF;

        // Execute
        if (!harness.step()) {
            if (!s_quiet) {
                debug << "FAIL " << test->name << ": Execution timeout (opcode 0x"
                      << std::hex << static_cast<int>(opcode_hi)
                      << std::setfill('0') << std::setw(2)
                      << static_cast<int>(harness.get_memory(test->initial.pc + 1))
                      << ")\n" << std::dec;
                output_.add(debug.str());
            }
            results_.failed_tests++;
            results_.record_opcode_result(opcode_hi, false);
            return false;
        }

        // Compare state
        bool state_ok = true;
        bool cycle_ok = true;

        m68k_cpu_state_t actual;
        harness.read_state(&actual);
        const m68k_cpu_state_t& expected = test->final_state;

        // Register comparison macro
        #define CHECK_REG(reg_name, actual_val, expected_val) \
            if ((actual_val) != (expected_val)) { \
                if (!s_quiet) { \
                    debug << "FAIL " << test->name << ": " << reg_name << " - expected 0x" \
                          << std::hex << (expected_val) \
                          << ", got 0x" << (actual_val) << std::dec << "\n"; \
                } \
                state_ok = false; \
            }

        // Data registers
        CHECK_REG("D0", actual.d[0], expected.d[0]);
        CHECK_REG("D1", actual.d[1], expected.d[1]);
        CHECK_REG("D2", actual.d[2], expected.d[2]);
        CHECK_REG("D3", actual.d[3], expected.d[3]);
        CHECK_REG("D4", actual.d[4], expected.d[4]);
        CHECK_REG("D5", actual.d[5], expected.d[5]);
        CHECK_REG("D6", actual.d[6], expected.d[6]);
        CHECK_REG("D7", actual.d[7], expected.d[7]);

        // Address registers
        CHECK_REG("A0", actual.a[0], expected.a[0]);
        CHECK_REG("A1", actual.a[1], expected.a[1]);
        CHECK_REG("A2", actual.a[2], expected.a[2]);
        CHECK_REG("A3", actual.a[3], expected.a[3]);
        CHECK_REG("A4", actual.a[4], expected.a[4]);
        CHECK_REG("A5", actual.a[5], expected.a[5]);
        CHECK_REG("A6", actual.a[6], expected.a[6]);

        // Stack pointers and control
        CHECK_REG("USP", actual.usp, expected.usp);
        CHECK_REG("SSP", actual.ssp, expected.ssp);
        CHECK_REG("SR", actual.sr, expected.sr);
        CHECK_REG("PC", actual.pc, expected.pc);

        // Prefetch pipeline (if test data provides it)
        if (expected.has_prefetch) {
            CHECK_REG("prefetch[0]/IR",  actual.prefetch[0], expected.prefetch[0]);
            CHECK_REG("prefetch[1]/IRC", actual.prefetch[1], expected.prefetch[1]);
        }

        #undef CHECK_REG

        // Memory comparison
        for (uint16_t i = 0; i < expected.ram_count; i++) {
            uint8_t actual_val = harness.get_memory(expected.ram[i].address);
            if (actual_val != expected.ram[i].value) {
                if (!s_quiet) {
                    debug << "FAIL " << test->name << ": Memory[0x" << std::hex
                          << expected.ram[i].address << "] - expected 0x"
                          << static_cast<int>(expected.ram[i].value)
                          << ", got 0x" << static_cast<int>(actual_val) << std::dec << "\n";
                }
                state_ok = false;
            }
        }

        // Cycle count comparison
        if (test->has_length) {
            uint32_t actual_cycles = harness.get_cycle_count();
            if (actual_cycles != test->length) {
                if (!s_quiet) {
                    debug << "FAIL " << test->name << ": Cycle count - expected "
                          << test->length << ", got " << actual_cycles << "\n";
                }
                cycle_ok = false;
            }
        }

        // Record result
        if (state_ok && cycle_ok) {
            results_.passed_tests++;
            results_.record_opcode_result(opcode_hi, true);
            if (s_verbose && !suppress_verbose) {
                debug << "PASS " << test->name << " (opcode 0x" << std::hex
                      << std::setfill('0') << std::setw(2)
                      << static_cast<int>(opcode_hi) << ")\n" << std::dec;
                output_.add(debug.str());
            }
            return true;
        } else {
            results_.failed_tests++;
            if (!state_ok) results_.state_mismatches++;
            if (!cycle_ok) results_.cycle_mismatches++;
            results_.record_opcode_result(opcode_hi, false);
            if (!s_quiet) {
                debug << "FAIL " << test->name << ": ";
                if (!state_ok) debug << "State ";
                if (!cycle_ok) debug << "Cycle ";
                debug << "mismatch (opcode 0x" << std::hex
                      << std::setfill('0') << std::setw(2)
                      << static_cast<int>(opcode_hi) << ")\n" << std::dec;
                output_.add(debug.str());
            }
            return false;
        }
    }
};

// ============================================================================
// File collection
// ============================================================================

// Decompress a gzip (or any libarchive-supported) file into a string
static bool decompress_gz(const std::string& filepath, std::string& output) {
    struct archive* a = archive_read_new();
    if (!a) return false;

    archive_read_support_filter_all(a);
    archive_read_support_format_raw(a);  // raw = single-stream (gz, bz2, xz, …)

    if (archive_read_open_filename(a, filepath.c_str(), 65536) != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }

    struct archive_entry* entry;
    if (archive_read_next_header(a, &entry) != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }

    output.clear();
    const void* block;
    size_t block_size;
    la_int64_t offset;
    while (archive_read_data_block(a, &block, &block_size, &offset) == ARCHIVE_OK) {
        output.append(static_cast<const char*>(block), block_size);
    }

    archive_read_free(a);
    return true;
}

static void collect_tests_from_file(const std::string& filepath, std::vector<TestItem>& tests) {
    tests.clear();

    std::string content;
    bool is_gz = (filepath.size() > 3 && filepath.substr(filepath.size() - 3) == ".gz");

    if (is_gz) {
        if (!decompress_gz(filepath, content)) {
            std::cout << "ERROR: Could not decompress file: " << filepath << "\n";
            return;
        }
    } else {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cout << "ERROR: Could not open file: " << filepath << "\n";
            return;
        }
        auto size = file.tellg();
        if (size <= 0) return;
        file.seekg(0, std::ios::beg);
        content.resize(static_cast<size_t>(size));
        if (!file.read(&content[0], size)) return;
        file.close();
    }

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
                        (entry.path().extension() == ".json" || entry.path().extension() == ".gz")) {
                        if (!opcode_filter.empty()) {
                            // For .json.gz files, stem() gives "NOP.json", need to strip .json too
                            std::string stem = entry.path().stem().string();
                            if (stem.size() > 5 && stem.substr(stem.size() - 5) == ".json")
                                stem = stem.substr(0, stem.size() - 5);
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
    std::cout << "M680x0 SingleStepTests Runner\n";
    std::cout << "Usage: " << prog << " [options] <test_file_or_directory>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -p, --processor P  Processor variant: mc68000 (default), mc68010, mc68020\n";
    std::cout << "  -v, --verbose      Verbose output\n";
    std::cout << "  -q, --quiet        Quiet mode (summary only)\n";
    std::cout << "  -c, --continue     Continue after failures\n";
    std::cout << "  -s, --stop-first   Stop on first failure (default)\n";
    std::cout << "  -j, --jobs N       Worker threads (default: cores-1)\n";
    std::cout << "  -o, --opcode HH    Filter to single opcode file (e.g. 4e71 for NOP)\n";
    std::cout << "  -h, --help         This message\n\n";
    std::cout << "Test Suite: https://github.com/SingleStepTests/m68000\n";
    std::cout << "Place the test directory somewhere accessible and pass its path.\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " path/to/m68000/v1/\n";
    std::cout << "  " << prog << " -o 4e71 -v path/to/m68000/v1/\n";
    std::cout << "  " << prog << " -j 8 -c path/to/m68000/v1/\n";
}

static void print_results(const TestResults& r, std::chrono::milliseconds dur,
                           size_t workers, size_t collected, const char* cpu_name) {
    std::cout << "\n=== M680x0 PROCESSOR TESTS RESULTS ===\n";
    std::cout << "CPU: " << cpu_name << "\n";
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

        std::cout << "\nFailing opcode groups (upper byte):\n";
        uint32_t failing = 0;
        for (int i = 0; i < 256; i++) {
            if (r.opcode_failures[i] > 0) {
                std::cout << "  0x" << std::hex << std::setfill('0') << std::setw(2) << i
                          << ": " << std::dec << r.opcode_failures[i]
                          << "/" << r.opcode_totals[i] << " failed\n";
                failing++;
            }
        }
        std::cout << "Total failing opcode groups: " << failing << "\n";
    }

    if (r.passed_tests == r.total_tests && r.total_tests > 0)
        std::cout << "\nALL TESTS PASSED - M680x0 implementation is hardware-accurate!\n";
    else if (r.failed_tests > 0)
        std::cout << "\nSOME TESTS FAILED - implementation differs from reference\n";
}

// ============================================================================
// Main entry point
// ============================================================================

template <typename CPU>
static int run_all_tests(const std::vector<std::string>& test_paths,
                         const std::string& opcode_filter,
                         size_t num_workers,
                         const char* cpu_name) {
    std::cout << "=== M680x0 SingleStepTests Runner ===\n";
    std::cout << "CPU: " << cpu_name << "\n";
    std::cout << "Worker threads: " << num_workers << "\n";
    if (!opcode_filter.empty())
        std::cout << "Opcode filter: " << opcode_filter << "\n";
    std::cout << "Verbose: " << (s_verbose ? "yes" : "no") << "\n";
    std::cout << "Quiet: " << (s_quiet ? "yes" : "no") << "\n";
    std::cout << "Stop on failure: " << (s_stop_on_failure ? "yes" : "no") << "\n\n";

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
    M68KTestWorkerPool<CPU> pool(effective, output, thread_results, s_test_failed);

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
    print_results(results, dur, effective, all_tests.size(), cpu_name);

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
    std::string processor = "mc68000";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            s_verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            s_quiet = true;
        } else if (arg == "-c" || arg == "--continue") {
            s_stop_on_failure = false;
        } else if (arg == "-s" || arg == "--stop-first") {
            s_stop_on_failure = true;
        } else if (arg == "-j" || arg == "--jobs") {
            if (i + 1 < argc) num_workers = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "-o" || arg == "--opcode") {
            if (i + 1 < argc) {
                opcode_filter = argv[++i];
                // Normalize: remove 0x prefix and convert to lowercase
                if (opcode_filter.size() > 2 &&
                    (opcode_filter.substr(0, 2) == "0x" || opcode_filter.substr(0, 2) == "0X"))
                    opcode_filter = opcode_filter.substr(2);
                std::transform(opcode_filter.begin(), opcode_filter.end(), opcode_filter.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            }
        } else if (arg == "-p" || arg == "--processor") {
            if (i + 1 < argc) {
                processor = argv[++i];
                std::transform(processor.begin(), processor.end(), processor.begin(),
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

    // Dispatch to the selected processor variant
    if (processor == "mc68000" || processor == "68000") {
        return run_all_tests<MC68000>(test_paths, opcode_filter, num_workers, "Motorola MC68000");
    } else if (processor == "mc68010" || processor == "68010") {
        return run_all_tests<m680x0::m680x0_t<m680x0::MC68010Traits>>(
            test_paths, opcode_filter, num_workers, "Motorola MC68010");
    } else if (processor == "mc68020" || processor == "68020") {
        return run_all_tests<m680x0::m680x0_t<m680x0::MC68020Traits>>(
            test_paths, opcode_filter, num_workers, "Motorola MC68020");
    } else {
        std::cout << "ERROR: Unknown processor variant: " << processor << "\n";
        std::cout << "Valid options: mc68000, mc68010, mc68020\n";
        return 1;
    }
}
