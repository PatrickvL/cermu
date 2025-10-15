#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <bitset>
#include <cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <sstream>
#include <future>

extern "C" {
#include "json_parser.h"
}

#ifndef CHIPS_IMPL
    #define CHIPS_IMPL
#endif

// Include processor-specific headers
#include "../src/chip/cpu/fam65xx/mos6502.hpp"

namespace fs = std::filesystem;

// Test results tracking with enhanced statistics
struct TestResults {
    uint32_t total_tests = 0;
    uint32_t passed_tests = 0;
    uint32_t failed_tests = 0;
    uint32_t cycle_mismatches = 0;
    uint32_t state_mismatches = 0;
    uint32_t bus_cycle_mismatches = 0;
    uint32_t opcode_failures[256] = {0};
    uint32_t opcode_totals[256] = {0};
};

// Thread-safe test results accumulator
struct ThreadSafeTestResults {
    std::atomic<uint32_t> total_tests{0};
    std::atomic<uint32_t> passed_tests{0};
    std::atomic<uint32_t> failed_tests{0};
    std::atomic<uint32_t> cycle_mismatches{0};
    std::atomic<uint32_t> state_mismatches{0};
    std::atomic<uint32_t> bus_cycle_mismatches{0};
    
    // Thread-safe opcode counters
    std::mutex opcode_mutex;
    uint32_t opcode_failures[256] = {0};
    uint32_t opcode_totals[256] = {0};
    
    void record_opcode_result(uint8_t opcode_val, bool success) {
        std::lock_guard<std::mutex> lock(opcode_mutex);
        opcode_totals[opcode_val]++;
        if (!success) {
            opcode_failures[opcode_val]++;
        }
    }
    
    void merge_into_global(TestResults& global_results) {
        global_results.total_tests = total_tests.load();
        global_results.passed_tests = passed_tests.load();
        global_results.failed_tests = failed_tests.load();
        global_results.cycle_mismatches = cycle_mismatches.load();
        global_results.state_mismatches = state_mismatches.load();
        global_results.bus_cycle_mismatches = bus_cycle_mismatches.load();
        
        std::lock_guard<std::mutex> lock(opcode_mutex);
        for (int i = 0; i < 256; i++) {
            global_results.opcode_failures[i] = opcode_failures[i];
            global_results.opcode_totals[i] = opcode_totals[i];
        }
    }
};

// Thread-safe output buffer for preventing mixed stdout
class ThreadSafeOutput {
private:
    std::mutex output_mutex;
    std::queue<std::string> output_queue;
    std::atomic<bool> should_flush{false};
    
public:
    void add_output(const std::string& output) {
        std::lock_guard<std::mutex> lock(output_mutex);
        output_queue.push(output);
        should_flush = true;
    }
    
    void flush_all() {
        std::lock_guard<std::mutex> lock(output_mutex);
        while (!output_queue.empty()) {
            std::cout << output_queue.front();
            output_queue.pop();
        }
        should_flush = false;
    }
    
    bool has_pending() const {
        return should_flush.load();
    }
};

// Updated test harness using the new MOS 6502 template-based API
class ProcessorTestHarness {
private:
    mos6502_c_t cpu;  // Use the new C wrapper type
    uint8_t memory[65536];
    uint32_t cycle_count;
    uint64_t pins;  // Maintain pins state across steps
    
    // Bus cycle tracking for comparing against JSON test data - reserve capacity to avoid reallocations
    std::vector<bus_cycle_t> actual_bus_cycles;
    
    // Memory callbacks - reliable approach from C++ version
    static uint8_t mem_read(void* user_data, uint16_t addr, uint8_t bus_state) {
        (void)bus_state; // Suppress unused parameter warning
        ProcessorTestHarness* harness = static_cast<ProcessorTestHarness*>(user_data);
        uint8_t value = harness->memory[addr];
        
        // Record bus cycle for tracking
        harness->record_bus_cycle(addr, value, false);
        
        return value;
    }
    
    static void mem_write(void* user_data, uint16_t addr, uint8_t data) {
        ProcessorTestHarness* harness = static_cast<ProcessorTestHarness*>(user_data);
        harness->memory[addr] = data;
        
        // Record bus cycle for tracking
        harness->record_bus_cycle(addr, data, true);
    }
    
    // Record bus cycle for comparison with JSON test data
    void record_bus_cycle(uint16_t addr, uint8_t data, bool is_write) {
        bus_cycle_t cycle;
        cycle.address = addr;
        cycle.data = data;
        cycle.is_write = is_write;
        actual_bus_cycles.push_back(cycle);
        
        // Removed verbose output for performance - handled in threaded version
    }

public:
    // Bootstrap processor for ProcessorTests compatibility
    void bootstrap_processor_for_tests() {
        pins = mos6502_reset(&cpu, pins);  // Use reset instead of bootstrap
    }

    ProcessorTestHarness() : cycle_count(0) {
        // Clear memory (optimized approach from C version)
        std::fill(memory, memory + 65536, 0);
        
        // Initialize CPU with memory callbacks using new API
        fam65xx_desc_t desc = {};
        desc.mem_read = mem_read;
        desc.mem_write = mem_write;
        desc.mem_user_data = this;
        
        pins = mos6502_init(&cpu, &desc);  // Use mos6502_init instead of fam65xx_init
        
        // ProcessorTests expects CPU to be ready for immediate execution
        cycle_count = 0;
    }
    
    // Clear memory based on bus cycle writes and test data
    void clear_written_memory(const cpu_state_t* test_data) {
        // Clear test data addresses if provided
        if (test_data && test_data->ram_count > 0) {
            for (int i = 0; i < test_data->ram_count; i++) {
                for (int j = 0; j < test_data->ram[i].byte_count; j++) {
                    memory[test_data->ram[i].address + j] = 0;
                }
            }
        }

        // Clear memory based on bus cycle writes
        for (const auto& cycle : actual_bus_cycles) {
            if (cycle.is_write) {
                memory[cycle.address] = 0;
            }
        }
    }
    
    // Setup memory for new test with optimizations
    void setup_memory_for_test(const cpu_state_t* initial, const cpu_state_t* previous_final = nullptr) {
        // Clear bus cycle tracking for new test - reserve capacity to avoid reallocations
        clear_bus_cycles();
        
        // PERFORMANCE OPTIMIZATION: Only clear memory that was actually used in previous test
        if (previous_final != nullptr) {
            // Use optimized selective clearing
            clear_written_memory(previous_final);
        } else {
            // First test - clear all memory (constructor already did this, but be safe)
            std::fill(memory, memory + 65536, 0);
        }
        
        // Set up memory from RAM entries
        for (int i = 0; i < initial->ram_count; i++) {
            uint16_t addr = initial->ram[i].address;
            for (int j = 0; j < initial->ram[i].byte_count; j++) {
                memory[addr + j] = initial->ram[i].bytes[j];
            }
        }
    }
    
    // Clear bus cycle tracking - optimize for performance
    void clear_bus_cycles() {
        actual_bus_cycles.clear();
        // Reserve capacity for typical instruction (5-7 bus cycles max)
        actual_bus_cycles.reserve(8);
    }
    
    // Get recorded bus cycles
    const std::vector<bus_cycle_t>& get_bus_cycles() const {
        return actual_bus_cycles;
    }
    
    // CPU state accessors - use new MOS 6502 API
    void set_pc(uint16_t pc) { mos6502_set_pc(&cpu, pc); }
    void set_a(uint8_t a) { mos6502_set_a(&cpu, a); }
    void set_x(uint8_t x) { mos6502_set_x(&cpu, x); }
    void set_y(uint8_t y) { mos6502_set_y(&cpu, y); }
    void set_sp(uint8_t sp) { mos6502_set_s(&cpu, sp); }
    void set_status(uint8_t p) { mos6502_set_p(&cpu, p); }
    
    uint16_t get_pc() const { return mos6502_pc(const_cast<mos6502_c_t*>(&cpu)); }
    uint8_t get_a() const { return mos6502_a(const_cast<mos6502_c_t*>(&cpu)); }
    uint8_t get_x() const { return mos6502_x(const_cast<mos6502_c_t*>(&cpu)); }
    uint8_t get_y() const { return mos6502_y(const_cast<mos6502_c_t*>(&cpu)); }
    uint8_t get_sp() const { return mos6502_s(const_cast<mos6502_c_t*>(&cpu)); }
    uint8_t get_status() const { return mos6502_p(const_cast<mos6502_c_t*>(&cpu)); }
    
    // Memory access (for direct memory setup, not during CPU execution)
    void set_memory(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
    uint8_t get_memory(uint16_t addr) const { return memory[addr]; }
    
    // Cycle counting
    uint32_t get_cycle_count() const { return cycle_count; }
    void reset_cycle_count() { cycle_count = 0; }
    
    // Execute one instruction - SYNC-based completion detection (optimized for threading)
    bool step() {
        return step_with_debug(nullptr);
    }
    
    // Execute one instruction with detailed cycle logging for debugging
    bool step_with_debug(std::ostringstream* debug_output) {
        try {
            uint32_t max_cycles = 10; // Safety limit
            uint32_t cycle_in_instruction = 0;
            
            do {
                // Capture state before tick
                uint16_t pc_before = mos6502_pc(&cpu);
                uint8_t a_before = mos6502_a(&cpu);
                uint8_t x_before = mos6502_x(&cpu);
                uint8_t y_before = mos6502_y(&cpu);
                uint8_t s_before = mos6502_s(&cpu);
                uint8_t p_before = mos6502_p(&cpu);
                
                pins = mos6502_tick(&cpu, pins);  // Use mos6502_tick instead of fam65xx_tick
                cycle_count++;
                cycle_in_instruction++;
                
                // Capture state after tick
                uint16_t pc_after = mos6502_pc(&cpu);
                uint8_t a_after = mos6502_a(&cpu);
                uint8_t x_after = mos6502_x(&cpu);
                uint8_t y_after = mos6502_y(&cpu);
                uint8_t s_after = mos6502_s(&cpu);
                uint8_t p_after = mos6502_p(&cpu);
                
                // Log detailed cycle information if debug output provided
                if (debug_output) {
                    *debug_output << "    Cycle " << cycle_in_instruction << ": "
                                  << "PC 0x" << std::hex << std::setfill('0') << std::setw(4) << pc_before
                                  << "->0x" << pc_after << std::dec
                                  << " A:0x" << std::hex << std::setfill('0') << std::setw(2) << (int)a_before
                                  << "->0x" << (int)a_after << std::dec;
                    
                    // Show significant register changes
                    if (x_before != x_after || y_before != y_after || s_before != s_after || p_before != p_after) {
                        *debug_output << " X:0x" << std::hex << (int)x_before << "->0x" << (int)x_after
                                      << " Y:0x" << (int)y_before << "->0x" << (int)y_after
                                      << " S:0x" << (int)s_before << "->0x" << (int)s_after
                                      << " P:0x" << (int)p_before << "->0x" << (int)p_after << std::dec;
                    }
                    
                    // Show bus activity from last bus cycle
                    if (!actual_bus_cycles.empty()) {
                        const auto& last_cycle = actual_bus_cycles.back();
                        *debug_output << " Bus[0x" << std::hex << std::setfill('0') << std::setw(4)
                                      << last_cycle.address << "]=0x" << std::setw(2) << (int)last_cycle.data
                                      << (last_cycle.is_write ? "W" : "R") << std::dec;
                    }
                    
                    *debug_output << std::endl;
                }
                
                // Instruction completes when opdone() returns true
                bool instruction_done = mos6502_opdone(&cpu);  // Use mos6502_opdone instead of fam65xx_opdone
                
                max_cycles--;
                if (max_cycles == 0) {
                    if (debug_output) {
                        *debug_output << "    ERROR: Exceeded max cycle limit!" << std::endl;
                    }
                    return false; // Exceeded cycle limit
                }
                
                if (instruction_done) {
                    if (debug_output) {
                        *debug_output << "    Instruction completed after " << cycle_in_instruction
                                      << " cycles" << std::endl;
                    }
                    break; // Instruction completed
                }
            } while (true);
            
            return true;
        } catch (...) {
            return false;
        }
    }
};

// Test item for worker queue
struct TestItem {
    std::string filepath;
    std::string test_json;
    std::string test_name;
};

// Forward declaration for TestWorkerPool
class TestWorkerPool;

// Worker thread pool class
class TestWorkerPool {
private:
    std::vector<std::thread> workers;
    std::queue<TestItem> test_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> shutdown{false};
    
    ThreadSafeOutput& output_handler;
    ThreadSafeTestResults& results;
    bool verbose_mode;
    bool quiet_mode;
    std::atomic<bool>& global_test_failed;
    bool stop_on_failure;
    
    
public:
    TestWorkerPool(size_t num_workers, ThreadSafeOutput& output, ThreadSafeTestResults& res,
                   bool verbose, bool quiet, std::atomic<bool>& test_failed, bool stop_fail)
        : output_handler(output), results(res), verbose_mode(verbose), quiet_mode(quiet),
          global_test_failed(test_failed), stop_on_failure(stop_fail) {
        
        for (size_t i = 0; i < num_workers; ++i) {
            workers.emplace_back(&TestWorkerPool::worker_thread, this, i);
        }
    }
    
    ~TestWorkerPool() {
        shutdown = true;
        queue_cv.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    void add_test(const TestItem& item) {
        std::lock_guard<std::mutex> lock(queue_mutex);
        test_queue.push(item);
        queue_cv.notify_one();
    }
    
    void wait_completion() {
        // Wait for queue to empty
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, [this] { return test_queue.empty(); });
    }
    
private:
    void worker_thread(size_t worker_id) {
        // PERFORMANCE OPTIMIZATION: Create one harness per worker thread
        // Reuse the same harness for all tests in this thread to avoid repeated initialization
        ProcessorTestHarness harness;
        processor_test_t* previous_test = nullptr;
        
        while (!shutdown) {
            TestItem item;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this] { return !test_queue.empty() || shutdown; });
                
                if (shutdown) break;
                if (test_queue.empty()) continue;
                
                item = test_queue.front();
                test_queue.pop();
            }
            
            // Check if we should stop due to failure
            if (stop_on_failure && global_test_failed.load()) {
                break;
            }
            
            // Process the test with reused harness
            process_single_test(item, worker_id, &harness, previous_test);
        }
    }
    
    void process_single_test(const TestItem& item, size_t worker_id, ProcessorTestHarness* harness, processor_test_t*& previous_test) {
        std::ostringstream thread_output;
        
        // Parse and run the test
        processor_test_t test;
        if (!json_parse_processor_test(item.test_json.c_str(), &test)) {
            thread_output << "ERROR: Failed to parse test in file: " << item.filepath << std::endl;
            output_handler.add_output(thread_output.str());
            return;
        }
        
        bool test_result = run_processor_test_threaded(&test, thread_output, worker_id, harness, previous_test);
        
        // Update previous test for next iteration's memory optimization
        if (previous_test) {
            *previous_test = test; // Copy test data for next iteration
        } else {
            previous_test = new processor_test_t(test); // First test for this thread
        }
        
        // Add output to thread-safe handler
        if (!thread_output.str().empty()) {
            output_handler.add_output(thread_output.str());
        }
        
        // Check if we should signal global failure
        if (!test_result && stop_on_failure) {
            global_test_failed = true;
        }
    }
    
    bool compare_bus_cycles_threaded(const std::vector<bus_cycle_t>& actual,
                                   const cpu_state_t& expected,
                                   const std::string& test_name,
                                   std::ostringstream& output) {
        if (!expected.has_bus_cycles) {
            return true;
        }
        
        bool match = true;
        
        if (actual.size() != expected.bus_cycle_count) {
            if (!quiet_mode) {
                output << "FAIL " << test_name << ": Bus cycle count - expected "
                       << (int)expected.bus_cycle_count << ", got " << actual.size() << std::endl;
            }
            match = false;
        }
        
        // Detailed bus cycle comparison with mismatch reporting
        size_t min_cycles = std::min(actual.size(), (size_t)expected.bus_cycle_count);
        for (size_t i = 0; i < min_cycles; i++) {
            const bus_cycle_t& actual_cycle = actual[i];
            const bus_cycle_t& expected_cycle = expected.bus_cycles[i];
            
            if (actual_cycle.address != expected_cycle.address ||
                actual_cycle.data != expected_cycle.data ||
                actual_cycle.is_write != expected_cycle.is_write) {
                
                if (!quiet_mode) {
                    output << "FAIL " << test_name << ": Bus cycle " << (i+1) << " mismatch:" << std::endl;
                    output << "  Expected: addr=0x" << std::hex << std::setfill('0') << std::setw(4)
                           << expected_cycle.address << " data=0x" << std::setw(2) << (int)expected_cycle.data
                           << (expected_cycle.is_write ? "W" : "R") << std::endl;
                    output << "  Actual:   addr=0x" << std::setw(4) << actual_cycle.address
                           << " data=0x" << std::setw(2) << (int)actual_cycle.data
                           << (actual_cycle.is_write ? "W" : "R") << std::dec << std::endl;
                }
                match = false;
                break; // Early exit for performance
            }
        }
        
        return match;
    }
    
    bool run_processor_test_threaded(const processor_test_t* test, std::ostringstream& output, size_t worker_id,
                                    ProcessorTestHarness* harness, const processor_test_t* previous_test) {
        results.total_tests++;
        
        if (verbose_mode) {
            output << "[Worker " << worker_id << "] Running test: " << test->name << std::endl;
        }
        
        // PERFORMANCE OPTIMIZATION: Use selective memory clearing based on previous test
        const cpu_state_t* previous_final = previous_test ? &previous_test->final : nullptr;
        harness->setup_memory_for_test(&test->initial, previous_final);
        
        // CRITICAL FIX: Bootstrap CPU to reset state before setting test state
        harness->bootstrap_processor_for_tests();
        
        harness->set_pc(test->initial.pc);
        harness->set_a(test->initial.a);
        harness->set_x(test->initial.x);
        harness->set_y(test->initial.y);
        harness->set_sp(test->initial.s);
        harness->set_status(test->initial.p);

        uint16_t pc_addr = test->initial.pc;
        uint8_t current_opcode = harness->get_memory(pc_addr);
        
        if (verbose_mode) {
            output << "  [Worker " << worker_id << "] Opcode at PC 0x" << std::hex << test->initial.pc
                   << ": 0x" << std::hex << (int)current_opcode << std::dec << std::endl;
        }
        
        uint32_t initial_cycle_count = harness->get_cycle_count();
        
        // Execute instruction (CPU already bootstrapped and configured)
        bool step_result;
        if (verbose_mode) {
            output << "  [Worker " << worker_id << "] Executing instruction with cycle-by-cycle details:" << std::endl;
            step_result = harness->step_with_debug(&output);
        } else {
            step_result = harness->step();
        }
        
        uint32_t cycles_executed = harness->get_cycle_count() - initial_cycle_count;
        
        if (verbose_mode) {
            output << "  [Worker " << worker_id << "] Final state: PC=0x" << std::hex
                   << harness->get_pc() << " A=0x" << (int)harness->get_a()
                   << " Cycles=" << std::dec << cycles_executed << std::endl;
        }
        
        if (!step_result) {
            if (!quiet_mode) {
                output << "FAIL " << test->name << ": Instruction execution failed (opcode 0x"
                       << std::hex << (int)current_opcode << ")" << std::dec << std::endl;
            }
            results.failed_tests++;
            results.record_opcode_result(current_opcode, false); // Record execution failure
            return false;
        }
        
        // Compare results (same logic as original but thread-safe)
        bool state_match = true;
        bool cycle_match = true;
        bool bus_cycle_match = true;
        
        // Check registers
        uint16_t actual_pc = harness->get_pc();
        if (actual_pc != test->final.pc) {
            if (!quiet_mode) {
                output << "FAIL " << test->name << ": PC - expected 0x" << std::hex
                       << test->final.pc << ", got 0x" << actual_pc << std::dec << std::endl;
            }
            state_match = false;
        }
        
        // Additional register checks (abbreviated for space, but include all original checks)
        if (harness->get_sp() != test->final.s ||
            harness->get_a() != test->final.a ||
            harness->get_x() != test->final.x ||
            harness->get_y() != test->final.y ||
            harness->get_status() != test->final.p) {
            state_match = false;
            // Detailed failure reporting would go here
        }
        
        // Memory state comparison
        for (uint8_t i = 0; i < test->final.ram_count; i++) {
            uint16_t addr = test->final.ram[i].address;
            for (uint8_t j = 0; j < test->final.ram[i].byte_count; j++) {
                uint8_t expected_value = test->final.ram[i].bytes[j];
                uint8_t actual_value = harness->get_memory(addr + j);
                
                if (actual_value != expected_value) {
                    if (!quiet_mode) {
                        output << "FAIL " << test->name << ": Memory[0x" << std::hex << (addr + j)
                               << "] - expected 0x" << (int)expected_value
                               << ", got 0x" << (int)actual_value << std::dec << std::endl;
                    }
                    state_match = false;
                }
            }
        }
        
        // Check cycle count
        if (test->final.has_cycles && cycles_executed != test->final.cycles) {
            if (!quiet_mode) {
                output << "FAIL " << test->name << ": Cycles - expected " << test->final.cycles
                       << ", got " << cycles_executed << std::endl;
            }
            cycle_match = false;
            results.cycle_mismatches++;
        }
        
        // Bus cycle comparison (simplified)
        bus_cycle_match = compare_bus_cycles_threaded(harness->get_bus_cycles(), test->final, test->name, output);
        if (!bus_cycle_match) {
            results.bus_cycle_mismatches++;
        }
        
        // Update results
        if (state_match && cycle_match && bus_cycle_match) {
            results.passed_tests++;
            results.record_opcode_result(current_opcode, true); // Mark as successful
            if (verbose_mode) {
                output << "PASS " << test->name << " (opcode 0x" << std::hex
                       << (int)current_opcode << ")" << std::dec << std::endl;
            }
            return true;
        } else {
            results.failed_tests++;
            if (!state_match) results.state_mismatches++;
            results.record_opcode_result(current_opcode, false); // Record failure
            
            if (verbose_mode) {
                output << "FAIL " << test->name << ": ";
                if (!state_match) output << "State ";
                if (!cycle_match) output << "Cycle ";
                if (!bus_cycle_match) output << "BusCycle ";
                output << "mismatch (opcode 0x" << std::hex << (int)current_opcode << ")" << std::dec << std::endl;
            }
            
            return false;
        }
    }
};

// Global variables
bool verbose_output = false;
static bool g_quiet_mode = false;
static bool g_stop_on_failure = true;
static std::atomic<bool> g_test_failed{false};
static TestResults results;

// Parallel file processing
std::vector<TestItem> collect_tests_from_file(const std::string& filepath) {
    std::vector<TestItem> tests;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "ERROR: Could not open file: " << filepath << std::endl;
        return tests;
    }
    
    // Read entire file
    std::string json_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    // Parse JSON - handle both single tests and arrays
    const char* pos = json_content.c_str();
    pos = json_skip_whitespace(pos);
    
    if (*pos == '[') {
        // Array of tests
        pos++; // Skip opening bracket
        
        while (*pos) {
            pos = json_skip_whitespace(pos);
            if (*pos == ']') break;
            
            if (*pos == '{') {
                // Find the end of this test object
                const char* test_end = json_find_object_end(pos);
                if (!test_end) break;
                
                // Extract this test
                size_t test_len = test_end - pos + 1;
                std::string test_json(pos, test_len);
                
                TestItem item;
                item.filepath = filepath;
                item.test_json = test_json;
                item.test_name = "test_" + std::to_string(tests.size());
                tests.push_back(item);
                
                pos = test_end + 1;
            } else {
                break;
            }
            
            // Skip comma if present
            pos = json_skip_whitespace(pos);
            if (*pos == ',') pos++;
        }
    } else {
        // Single test
        TestItem item;
        item.filepath = filepath;
        item.test_json = json_content;
        item.test_name = "single_test";
        tests.push_back(item);
    }
    
    return tests;
}

// Optimized directory processing for parallel execution
std::vector<TestItem> collect_all_tests(const std::vector<std::string>& test_paths) {
    std::vector<TestItem> all_tests;
    
    for (const auto& test_path : test_paths) {
        try {
            if (fs::is_directory(test_path)) {
                for (const auto& entry : fs::recursive_directory_iterator(test_path)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".json") {
                        auto file_tests = collect_tests_from_file(entry.path().string());
                        all_tests.insert(all_tests.end(), file_tests.begin(), file_tests.end());
                    }
                }
            } else if (fs::is_regular_file(test_path)) {
                auto file_tests = collect_tests_from_file(test_path);
                all_tests.insert(all_tests.end(), file_tests.begin(), file_tests.end());
            } else {
                std::cout << "ERROR: Invalid path: " << test_path << std::endl;
            }
        } catch (const fs::filesystem_error& ex) {
            std::cout << "ERROR: Could not access path: " << test_path 
                      << " (" << ex.what() << ")" << std::endl;
        }
    }
    
    return all_tests;
}

// Enhanced usage information
void print_usage(const char* program_name) {
    std::cout << "fam65xx ProcessorTests Runner - Template Edition\n";
    std::cout << "Usage: " << program_name << " [options] <test_file_or_directory>\n";
    std::cout << "\nTest Execution Options:\n";
    std::cout << "  -v, --verbose      Enable verbose output with detailed execution logs\n";
    std::cout << "  -q, --quiet        Quiet mode - only show final summary\n";
    std::cout << "  -c, --continue     Continue testing after failures (default: stop on first failure)\n";
    std::cout << "  -s, --stop-first   Stop on first failure (default behavior)\n";
    std::cout << "  -j, --jobs N       Number of parallel jobs (default: CPU cores - 1)\n";
    std::cout << "  -h, --help         Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " processor_tests/nes6502/v1/                    # Run with default parallelism\n";
    std::cout << "  " << program_name << " -j 4 -v processor_tests/nes6502/v1/69.json   # 4 workers, verbose output\n";
    std::cout << "  " << program_name << " -q -c -j 8 processor_tests/nes6502/v1/       # 8 workers, quiet, continue on failures\n";
    std::cout << "\nFeatures:\n";
    std::cout << "  ✓ New template-based MOS 6502 CPU implementation\n";
    std::cout << "  ✓ Parallel test execution for maximum performance\n";
    std::cout << "  ✓ Thread-safe output (no mixed stdout)\n";
    std::cout << "  ✓ Intelligent core usage (hardware cores - 1)\n";
    std::cout << "  ✓ Hardware-accurate timing validation\n";
}

// Enhanced results printing
void print_results(std::chrono::milliseconds duration, size_t num_workers) {
    std::cout << "\n=== FAM65XX PROCESSOR TESTS RESULTS (Template Edition) ===\n";
    std::cout << "CPU Implementation: Template-based MOS 6502\n";
    std::cout << "Execution time: " << duration.count() << " ms\n";
    std::cout << "Worker threads: " << num_workers << "\n";
    std::cout << "Total tests run: " << results.total_tests << "\n";
    std::cout << "Tests passed: " << results.passed_tests << "\n";
    std::cout << "Tests failed: " << results.failed_tests << "\n";
    
    if (results.total_tests > 0) {
        double pass_rate = (double)results.passed_tests / results.total_tests * 100.0;
        std::cout << "Pass rate: " << std::fixed << std::setprecision(2) << pass_rate << "%\n";
        
        // Calculate tests per second
        double tests_per_second = (double)results.total_tests / (duration.count() / 1000.0);
        std::cout << "Performance: " << std::fixed << std::setprecision(1) << tests_per_second << " tests/second\n";
    }
    
    if (results.failed_tests > 0) {
        std::cout << "\nFailure breakdown:\n";
        std::cout << "State mismatches: " << results.state_mismatches << "\n";
        std::cout << "Cycle mismatches: " << results.cycle_mismatches << "\n";
        std::cout << "Bus cycle mismatches: " << results.bus_cycle_mismatches << "\n";
        
        std::cout << "\nFailing opcodes:\n";
        uint32_t failing_opcodes = 0;
        for (int i = 0; i < 256; i++) {
            if (results.opcode_failures[i] > 0) {
                std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) << i 
                          << ": " << std::dec << results.opcode_failures[i] 
                          << "/" << results.opcode_totals[i] << " failed\n";
                failing_opcodes++;
            }
        }
        std::cout << "Total failing opcodes: " << failing_opcodes << "\n";
    }
    
    if (results.passed_tests == results.total_tests) {
        std::cout << "\n🎉 ALL TESTS PASSED - Template-based fam65xx is hardware-accurate! 🎉\n";
    } else {
        std::cout << "\n❌ SOME TESTS FAILED - implementation differs from hardware\n";
    }
}

// Main function with parallel execution
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::vector<std::string> test_paths;
    size_t num_workers = std::max(1u, std::thread::hardware_concurrency() - 1); // CPU cores - 1
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose_output = true;
        } else if (arg == "-q" || arg == "--quiet") {
            g_quiet_mode = true;
        } else if (arg == "-c" || arg == "--continue") {
            g_stop_on_failure = false;
        } else if (arg == "-s" || arg == "--stop-first") {
            g_stop_on_failure = true;
        } else if (arg == "-j" || arg == "--jobs") {
            if (i + 1 < argc) {
                num_workers = std::max(1, std::atoi(argv[++i]));
            }
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            test_paths.push_back(arg);
        }
    }
    
    if (test_paths.empty()) {
        std::cout << "ERROR: No test file or directory specified\n";
        print_usage(argv[0]);
        return 1;
    }
    
    std::cout << "=== fam65xx ProcessorTests Runner - Template Edition ===\n";
    std::cout << "CPU Implementation: Template-based MOS 6502\n";
    std::cout << "Test paths: " << test_paths.size() << " specified\n";
    std::cout << "Worker threads: " << num_workers << "\n";
    std::cout << "Verbose: " << (verbose_output ? "enabled" : "disabled") << "\n";
    std::cout << "Quiet mode: " << (g_quiet_mode ? "enabled" : "disabled") << "\n";
    std::cout << "Stop on failure: " << (g_stop_on_failure ? "enabled" : "disabled") << "\n\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Collect all tests first
    std::cout << "Collecting tests..." << std::flush;
    auto all_tests = collect_all_tests(test_paths);
    std::cout << " Found " << all_tests.size() << " tests\n";
    
    if (all_tests.empty()) {
        std::cout << "No tests found in specified paths!\n";
        return 1;
    }
    
    // Set up parallel execution
    ThreadSafeOutput output_handler;
    ThreadSafeTestResults thread_results;
    TestWorkerPool worker_pool(num_workers, output_handler, thread_results, 
                               verbose_output, g_quiet_mode, g_test_failed, g_stop_on_failure);
    
    // Submit all tests to worker pool
    std::cout << "Starting parallel execution...\n";
    for (const auto& test : all_tests) {
        worker_pool.add_test(test);
    }
    
    // Wait for completion and periodically flush output
    while (!all_tests.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Flush any pending output
        if (output_handler.has_pending()) {
            output_handler.flush_all();
        }
        
        // Check if we're done (simplified check)
        if (thread_results.total_tests.load() >= all_tests.size()) {
            break;
        }
        
        // Early exit on failure if requested
        if (g_stop_on_failure && g_test_failed.load()) {
            break;
        }
    }
    
    // Final output flush
    output_handler.flush_all();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Transfer results to global structure
    thread_results.merge_into_global(results);
    
    print_results(duration, num_workers);
    
    if (results.total_tests == 0) {
        std::cout << "\nNo tests were executed!\n";
        return 1;
    } else if (results.passed_tests == results.total_tests) {
        std::cout << "\nALL TESTS PASSED - Template-based fam65xx matches ProcessorTests ground truth!\n";
        return 0;
    } else {
        double pass_rate = (double)results.passed_tests / results.total_tests * 100.0;
        std::cout << "\nSOME TESTS FAILED - fam65xx pass rate: "
                  << std::fixed << std::setprecision(1) << pass_rate << "%\n";
        std::cout << "Implementation differs from hardware-verified ground truth\n";
        return 1;
    }
}