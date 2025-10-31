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
#include <map>
#include <stdexcept>

extern "C" {
#include "json_parser.h"
}

#ifndef AIEMUC_IMPL
    #define AIEMUC_IMPL
#endif

// Include new fam65xx processor implementation
#include "../src/chip/cpu/fam65xx/fam65xx.hpp"

namespace fs = std::filesystem;

// Thread-local memory for CPU testing - each worker gets its own memory space
thread_local uint8_t test_memory[65536];

// Global flags (declared early for use in functions)
static bool g_quiet_mode = false;
static bool g_stop_on_failure = true; 
static std::atomic<bool> g_test_failed{false};

// Forward declarations (moved later after full class definition)



// Processor type enumeration
enum class ProcessorType {
    MOS6502,
    NES6502, 
    MOS6510,
    WDC65C02,
    ROCKWELL65C02,
    WDC65C816
};

// Centralized processor name determination - single source of truth
std::string get_processor_name(ProcessorType type) {
    switch (type) {
        case ProcessorType::MOS6502:     return "MOS 6502";
        case ProcessorType::NES6502:     return "NES 6502 (Ricoh 2A03/2A07)";
        case ProcessorType::MOS6510:     return "MOS 6510 (C64)";
        case ProcessorType::WDC65C02:    return "WDC 65C02";
        case ProcessorType::ROCKWELL65C02: return "Rockwell 65C02";
        case ProcessorType::WDC65C816:   return "WDC 65C816";
        default:                         return "Unknown Processor";
    }
}

// Auto-detect processor type from test path
ProcessorType detect_processor_from_path(const std::string& test_path) {
    std::string path_lower = test_path;
    std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    // Normalize path separators for Windows/Unix compatibility
    std::replace(path_lower.begin(), path_lower.end(), '\\', '/');
    
    if (!g_quiet_mode) {
        fprintf(stderr, "DEBUG: Detecting processor from path: %s\n", test_path.c_str());
        fprintf(stderr, "DEBUG: Path lowercase: %s\n", path_lower.c_str());
    }
    
    if (path_lower.find("processor_tests/6502/") != std::string::npos) {
        if (!g_quiet_mode) printf("DEBUG: Detected MOS6502\n");
        return ProcessorType::MOS6502;
    }
    if (path_lower.find("processor_tests/nes6502/") != std::string::npos) {
        if (!g_quiet_mode) printf("DEBUG: Detected NES6502\n");
        return ProcessorType::NES6502;
    }
    if (path_lower.find("processor_tests/mos6510/") != std::string::npos) {
        if (!g_quiet_mode) printf("DEBUG: Detected MOS6510\n");
        return ProcessorType::MOS6510;
    }
    if (path_lower.find("processor_tests/wdc65c02/") != std::string::npos) {
        if (!g_quiet_mode) printf("DEBUG: Detected WDC65C02\n");
        return ProcessorType::WDC65C02;
    }
    if (path_lower.find("processor_tests/rockwell65c02/") != std::string::npos) {
        if (!g_quiet_mode) printf("DEBUG: Detected ROCKWELL65C02\n");
        return ProcessorType::ROCKWELL65C02;
    }
    if (path_lower.find("processor_tests/wdc65c816/") != std::string::npos) {
        if (!g_quiet_mode) printf("DEBUG: Detected WDC65C816\n");
        return ProcessorType::WDC65C816;
    }
    
    if (!g_quiet_mode) printf("DEBUG: Using default MOS6502\n");
    return ProcessorType::MOS6502;  // Default fallback
}

// Parse processor type from string (for command line option)
ProcessorType parse_processor_type(const std::string& processor_str) {
    std::string proc_lower = processor_str;
    std::transform(proc_lower.begin(), proc_lower.end(), proc_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (proc_lower == "mos6502" || proc_lower == "6502") return ProcessorType::MOS6502;
    if (proc_lower == "nes6502" || proc_lower == "nes") return ProcessorType::NES6502;
    if (proc_lower == "mos6510" || proc_lower == "6510") return ProcessorType::MOS6510;
    if (proc_lower == "wdc65c02" || proc_lower == "65c02") return ProcessorType::WDC65C02;
    if (proc_lower == "rockwell65c02" || proc_lower == "rockwell") return ProcessorType::ROCKWELL65C02;
    if (proc_lower == "wdc65c816" || proc_lower == "65c816") return ProcessorType::WDC65C816;
    
    throw std::invalid_argument("Unknown processor type: " + processor_str);
}

// Forward declarations
class ProcessorTestHarness;

// Unified processor interface that can work with any 65xx processor
class UnifiedProcessorInterface {
public:
    virtual ~UnifiedProcessorInterface() = default;
    virtual uint64_t init(const chip_descriptor_t* desc) = 0;
    virtual uint64_t bootstrap(uint64_t pins) = 0;
    virtual uint64_t tick(uint64_t pins) = 0;
    virtual bool opdone() = 0;
    
    // Register accessors
    virtual uint16_t get_pc() = 0;
    virtual uint8_t get_a() = 0;
    virtual uint8_t get_x() = 0;
    virtual uint8_t get_y() = 0;
    virtual uint8_t get_sp() = 0;
    virtual uint8_t get_status() = 0;
    
    virtual void set_pc(uint16_t pc) = 0;
    virtual void set_a(uint8_t a) = 0;
    virtual void set_x(uint8_t x) = 0;
    virtual void set_y(uint8_t y) = 0;
    virtual void set_sp(uint8_t sp) = 0;
    virtual void set_status(uint8_t p) = 0;
    
    // Set harness for bus cycle recording (thread-safe)
    virtual void set_harness(ProcessorTestHarness* harness) = 0;
};

// Forward declaration for factory function
std::unique_ptr<UnifiedProcessorInterface> create_processor(ProcessorType type);

// ProcessorWrapper template will be defined after ProcessorTestHarness

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



// Updated test harness using the unified processor wrapper
class ProcessorTestHarness {
private:
    std::unique_ptr<UnifiedProcessorInterface> cpu_wrapper;
    ProcessorType processor_type;
    uint8_t* memory;  // Point to global test_memory array
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
public:
    void record_bus_cycle(uint16_t addr, uint8_t data, bool is_write) {
        bus_cycle_t cycle;
        cycle.address = addr;
        cycle.data = data;
        cycle.is_write = is_write;
        actual_bus_cycles.push_back(cycle);
    }
    // Bootstrap processor for ProcessorTests compatibility
    void bootstrap_processor_for_tests() {
        pins = cpu_wrapper->bootstrap(pins);
    }

    ProcessorTestHarness(ProcessorType proc_type = ProcessorType::MOS6502)
        : processor_type(proc_type), memory(test_memory), cycle_count(0) {
        
        if (!g_quiet_mode) printf("DEBUG: ProcessorTestHarness constructor called for processor type %d\n", (int)proc_type);
        
        // Clear memory (optimized approach from C version)
        std::fill(memory, memory + 65536, static_cast<uint8_t>(0));
        
        if (!g_quiet_mode) printf("DEBUG: About to call create_processor...\n");
        
        // Create processor wrapper for the specified type
        cpu_wrapper = create_processor(processor_type);
        
        if (!g_quiet_mode) printf("DEBUG: create_processor returned successfully\n");
        
        // Set up harness for bus cycle recording - now works with all processors via unified interface
        cpu_wrapper->set_harness(this);
        
        if (!g_quiet_mode) printf("DEBUG: About to call cpu_wrapper->init...\n");
        
        // Initialize CPU with new API (memory callbacks handled differently)
        chip_descriptor_t desc = {};
        desc.description = "MOS6502 Test CPU";
        
        pins = cpu_wrapper->init(&desc);
        
        if (!g_quiet_mode) printf("DEBUG: ProcessorTestHarness constructor completed successfully\n");
        
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
            std::fill(memory, memory + 65536, static_cast<uint8_t>(0));
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
    
    // CPU state accessors - use unified processor wrapper
    void set_pc(uint16_t pc) { cpu_wrapper->set_pc(pc); }
    void set_a(uint8_t a) { cpu_wrapper->set_a(a); }
    void set_x(uint8_t x) { cpu_wrapper->set_x(x); }
    void set_y(uint8_t y) { cpu_wrapper->set_y(y); }
    void set_sp(uint8_t sp) { cpu_wrapper->set_sp(sp); }
    void set_status(uint8_t p) { cpu_wrapper->set_status(p); }
    
    // Thread-safe harness setting for bus cycle recording
    void set_harness_for_bus_recording() { cpu_wrapper->set_harness(this); }
    
    uint16_t get_pc() const { return cpu_wrapper->get_pc(); }
    uint8_t get_a() const { return cpu_wrapper->get_a(); }
    uint8_t get_x() const { return cpu_wrapper->get_x(); }
    uint8_t get_y() const { return cpu_wrapper->get_y(); }
    uint8_t get_sp() const { return cpu_wrapper->get_sp(); }
    uint8_t get_status() const { return cpu_wrapper->get_status(); }
    
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
                uint16_t pc_before = cpu_wrapper->get_pc();
                uint8_t a_before = cpu_wrapper->get_a();
                uint8_t x_before = cpu_wrapper->get_x();
                uint8_t y_before = cpu_wrapper->get_y();
                uint8_t s_before = cpu_wrapper->get_sp();
                uint8_t p_before = cpu_wrapper->get_status();
                
                pins = cpu_wrapper->tick(pins);
                cycle_count++;
                cycle_in_instruction++;
                
                // Capture state after tick
                uint16_t pc_after = cpu_wrapper->get_pc();
                uint8_t a_after = cpu_wrapper->get_a();
                uint8_t x_after = cpu_wrapper->get_x();
                uint8_t y_after = cpu_wrapper->get_y();
                uint8_t s_after = cpu_wrapper->get_sp();
                uint8_t p_after = cpu_wrapper->get_status();
                
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
                bool instruction_done = cpu_wrapper->opdone();
                
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

// Generic processor wrapper template - eliminates code duplication
template<const fam65xx::CPUTraits& Traits>
class ProcessorWrapper : public UnifiedProcessorInterface {
private:
    fam65xx::fam65xx_t<Traits>* cpu;
    chip_descriptor_t desc;
    void* harness_ptr; // Store harness for memory callbacks
    
    // Instance memory callbacks that know about this wrapper's harness
    static uint8_t instance_mem_read(void* user_data, uint16_t addr, uint8_t bus_state) {
        (void)bus_state; // Suppress unused parameter warning
        ProcessorWrapper<Traits>* wrapper = static_cast<ProcessorWrapper<Traits>*>(user_data);
        uint8_t value = test_memory[addr];
        if (wrapper->harness_ptr) {
            ProcessorTestHarness* harness = static_cast<ProcessorTestHarness*>(wrapper->harness_ptr);
            harness->record_bus_cycle(addr, value, false);
        }
        return value;
    }
    
    static void instance_mem_write(void* user_data, uint16_t addr, uint8_t data) {
        ProcessorWrapper<Traits>* wrapper = static_cast<ProcessorWrapper<Traits>*>(user_data);
        test_memory[addr] = data;
        if (wrapper->harness_ptr) {
            ProcessorTestHarness* harness = static_cast<ProcessorTestHarness*>(wrapper->harness_ptr);
            harness->record_bus_cycle(addr, data, true);
        }
    }
    
    // Helper function to get processor name for debug output
    const char* get_processor_debug_name() const {
        // Runtime identification based on CPUTraits features
        if (Traits.has_apu()) return "NES6502";
        else if (Traits.has_io_port()) return "MOS6510";
        else if (Traits.has(fam65xx::CPUCoreFlags::ROCKWELL_BITS)) return "Rockwell65C02";
        else if (Traits.has(fam65xx::CPUCoreFlags::C816_16BIT)) return "WDC65C816";
        else if (Traits.has(fam65xx::CPUCoreFlags::CMOS_BASE)) return "WDC65C02";
        else return "MOS6502";
    }

public:
    ProcessorWrapper() : harness_ptr(nullptr) {
        if (!g_quiet_mode) {
            printf("DEBUG: %sWrapper constructor called\n", get_processor_debug_name());
        }
        
        // Create CPU using C++ template implementation
        cpu = new fam65xx::fam65xx_t<Traits>();
        if (!cpu) {
            throw std::runtime_error("Failed to create CPU");
        }
        
        // Initialize CPU
        cpu->init(&desc);
        
        // Special handling for NES6502 - check if has APU via CPUTraits
        if constexpr (Traits.has_apu()) {
            cpu->set_processor_tests_mode(true);
            if (!g_quiet_mode) printf("DEBUG: Enabled ProcessorTests compatibility mode for NES6502\n");
        }
        
        // Set up memory callbacks with this wrapper as user_data
        cpu->set_memory_callbacks(instance_mem_read, instance_mem_write, this);
        
        if (!g_quiet_mode) {
            printf("DEBUG: %sWrapper created successfully\n", get_processor_debug_name());
        }
    }
    
    ~ProcessorWrapper() {
        if (cpu) {
            delete cpu;
        }
    }
    
    // Implement interface methods
    uint64_t init(const chip_descriptor_t* desc_ptr) override {
        if (desc_ptr) {
            desc = *desc_ptr;
        }
        // CPU already initialized in constructor
        return 0;
    }
    
    uint64_t bootstrap(uint64_t pins) override {
        return cpu->bootstrap(pins);
    }
    
    uint64_t tick(uint64_t pins) override {
        return cpu->tick(pins);
    }
    
    bool opdone() override {
        return cpu->opdone();
    }
    
    uint16_t get_pc() override {
        return cpu->reg16[REG_PC];
    }
    
    uint8_t get_a() override {
        return cpu->reg8[REG_A];
    }
    
    uint8_t get_x() override {
        return cpu->reg8[REG_X];
    }
    
    uint8_t get_y() override {
        return cpu->reg8[REG_Y];
    }
    
    uint8_t get_sp() override {
        return cpu->reg8[REG_S];
    }
    
    uint8_t get_status() override {
        return cpu->reg8[REG_P];
    }
    
    void set_pc(uint16_t pc) override {
        cpu->reg16[REG_PC] = pc;
    }
    
    void set_a(uint8_t a) override {
        cpu->reg8[REG_A] = a;
    }
    
    void set_x(uint8_t x) override {
        cpu->reg8[REG_X] = x;
    }
    
    void set_y(uint8_t y) override {
        cpu->reg8[REG_Y] = y;
    }
    
    void set_sp(uint8_t sp) override {
        cpu->reg8[REG_S] = sp;
    }
    
    void set_status(uint8_t p) override {
        cpu->reg8[REG_P] = p;
    }
    
    // Set harness for bus cycle recording (thread-safe)
    void set_harness(ProcessorTestHarness* harness) override {
        harness_ptr = harness;
    }
};

// Type aliases for convenience and backward compatibility
using MOS6502Wrapper = ProcessorWrapper<fam65xx::MOS6502>;
using NES6502Wrapper = ProcessorWrapper<fam65xx::RICOH_2A03>;
using WDC65C02Wrapper = ProcessorWrapper<fam65xx::WDC_W65C02S>;
using MOS6510Wrapper = ProcessorWrapper<fam65xx::MOS6510>;
using Rockwell65C02Wrapper = ProcessorWrapper<fam65xx::ROCKWELL_R65C02>;
using WDC65C816Wrapper = ProcessorWrapper<fam65xx::WDC_65C816>;

// Factory function to create processor instances
std::unique_ptr<UnifiedProcessorInterface> create_processor(ProcessorType type) {
    if (!g_quiet_mode) printf("DEBUG: Creating processor type: %d\n", (int)type);
    switch (type) {
        case ProcessorType::MOS6502:
            if (!g_quiet_mode) printf("DEBUG: Creating MOS6502Wrapper\n");
            return std::make_unique<MOS6502Wrapper>();
            
        case ProcessorType::NES6502:
            if (!g_quiet_mode) printf("DEBUG: Creating NES6502Wrapper\n");
            return std::make_unique<NES6502Wrapper>();
            
        case ProcessorType::WDC65C02:
            if (!g_quiet_mode) printf("DEBUG: Creating WDC65C02Wrapper\n");
            return std::make_unique<WDC65C02Wrapper>();
            
        case ProcessorType::MOS6510:
            if (!g_quiet_mode) printf("DEBUG: Creating MOS6510Wrapper\n");
            return std::make_unique<MOS6510Wrapper>();
            
        case ProcessorType::ROCKWELL65C02:
            if (!g_quiet_mode) printf("DEBUG: Creating Rockwell65C02Wrapper\n");
            return std::make_unique<Rockwell65C02Wrapper>();
            
        case ProcessorType::WDC65C816:
            if (!g_quiet_mode) printf("DEBUG: Creating WDC65C816Wrapper\n");
            return std::make_unique<WDC65C816Wrapper>();
            
        default:
            throw std::invalid_argument("Unsupported processor type");
    }
}

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
    std::atomic<size_t> active_workers{0};  // Track active workers
    std::atomic<size_t> total_tests_added{0};  // Track total tests added
    
    ThreadSafeOutput& output_handler;
    ThreadSafeTestResults& results;
    bool verbose_mode;
    bool quiet_mode;
    std::atomic<bool>& global_test_failed;
    bool stop_on_failure;
    ProcessorType processor_type;
    
    
public:
    TestWorkerPool(size_t num_workers, ThreadSafeOutput& output, ThreadSafeTestResults& res,
                   bool verbose, bool quiet, std::atomic<bool>& test_failed, bool stop_fail,
                   ProcessorType proc_type)
        : output_handler(output), results(res), verbose_mode(verbose), quiet_mode(quiet),
          global_test_failed(test_failed), stop_on_failure(stop_fail), processor_type(proc_type) {
        
        if (!quiet_mode) printf("DEBUG: Creating %zu worker threads...\n", num_workers);
        for (size_t i = 0; i < num_workers; ++i) {
            if (!quiet_mode) printf("DEBUG: Starting worker thread %zu\n", i);
            workers.emplace_back(&TestWorkerPool::worker_thread, this, i);
        }
        if (!quiet_mode) printf("DEBUG: All worker threads started\n");
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
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            test_queue.push(item);
            total_tests_added++;
        }
        queue_cv.notify_one();
    }
    
    void wait_completion() {
        // Wait for all tests to be processed by checking that:
        // 1. Queue is empty AND
        // 2. All workers are idle (no active workers) AND
        // 3. All tests have been completed
        while (true) {
            bool queue_empty;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                queue_empty = test_queue.empty();
            }
            
            // Check completion conditions
            bool all_tests_processed = results.total_tests.load() >= total_tests_added.load();
            bool no_active_workers = active_workers.load() == 0;
            
            if (queue_empty && no_active_workers && all_tests_processed) {
                break;
            }
            
            // Early exit on failure if requested
            if (stop_on_failure && global_test_failed.load()) {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
private:
    void worker_thread(size_t worker_id) {
        if (!quiet_mode) printf("DEBUG: Worker thread %zu starting, creating ProcessorTestHarness...\n", worker_id);
        
        // PERFORMANCE OPTIMIZATION: Create one harness per worker thread
        // Reuse the same harness for all tests in this thread to avoid repeated initialization
        ProcessorTestHarness harness(processor_type);  // Pass processor type to harness
        processor_test_t* previous_test = nullptr;
        
        if (!quiet_mode) printf("DEBUG: Worker thread %zu ProcessorTestHarness created successfully\n", worker_id);
        
        while (!shutdown) {
            TestItem item;
            bool got_item = false;
            
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait(lock, [this] { return !test_queue.empty() || shutdown; });
                
                if (shutdown) break;
                if (!test_queue.empty()) {
                    item = test_queue.front();
                    test_queue.pop();
                    got_item = true;
                    active_workers++;  // Mark this worker as active
                }
            }
            
            if (!got_item) continue;
            
            // Check if we should stop due to failure
            if (stop_on_failure && global_test_failed.load()) {
                active_workers--;  // Mark worker as inactive before breaking
                break;
            }
            
            // Process the test with reused harness
            process_single_test(item, worker_id, &harness, previous_test);
            
            active_workers--;  // Mark worker as inactive after processing
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
        
        // Capture all debug output in a separate buffer - only emit if test fails or verbose mode
        std::ostringstream debug_output;
        
        if (verbose_mode) {
            debug_output << "[Worker " << worker_id << "] Running test: " << test->name << std::endl;
        }
        
        // REMOVED: Set global harness (thread safety issue)
        // current_test_harness = harness;
        
        // PERFORMANCE OPTIMIZATION: Use selective memory clearing based on previous test
        const cpu_state_t* previous_final = previous_test ? &previous_test->final : nullptr;
        harness->setup_memory_for_test(&test->initial, previous_final);
        
        // CRITICAL FIX: Bootstrap CPU to reset state before setting test state
        harness->bootstrap_processor_for_tests();
        
        // THREAD SAFETY FIX: Set harness in processor wrapper for bus cycle recording
        harness->set_harness_for_bus_recording();
        
        harness->set_pc(test->initial.pc);
        harness->set_a(test->initial.a);
        harness->set_x(test->initial.x);
        harness->set_y(test->initial.y);
        harness->set_sp(test->initial.s);
        harness->set_status(test->initial.p);

        uint16_t pc_addr = test->initial.pc;
        uint8_t current_opcode = harness->get_memory(pc_addr);
        
        if (verbose_mode) {
            debug_output << "  [Worker " << worker_id << "] Opcode at PC 0x" << std::hex << test->initial.pc
                        << ": 0x" << std::hex << (int)current_opcode << std::dec << std::endl;
        }
        
        uint32_t initial_cycle_count = harness->get_cycle_count();
        
        // Execute instruction (CPU already bootstrapped and configured)
        bool step_result;
        if (verbose_mode) {
            debug_output << "  [Worker " << worker_id << "] Executing instruction with cycle-by-cycle details:" << std::endl;
            step_result = harness->step_with_debug(&debug_output);
        } else {
            step_result = harness->step();
        }
        
        uint32_t cycles_executed = harness->get_cycle_count() - initial_cycle_count;
        
        if (verbose_mode) {
            debug_output << "  [Worker " << worker_id << "] Final state: PC=0x" << std::hex
                        << harness->get_pc() << " A=0x" << (int)harness->get_a()
                        << " Cycles=" << std::dec << cycles_executed << std::endl;
        }
        
        if (!step_result) {
            if (!quiet_mode) {
                output << debug_output.str(); // Emit debug output on failure
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
                debug_output << "FAIL " << test->name << ": PC - expected 0x" << std::hex
                            << test->final.pc << ", got 0x" << actual_pc << std::dec << std::endl;
            }
            state_match = false;
        }
        
        // Additional register checks with detailed failure reporting
        if (harness->get_sp() != test->final.s) {
            if (!quiet_mode) {
                debug_output << "FAIL " << test->name << ": SP - expected 0x" << std::hex
                            << (int)test->final.s << ", got 0x" << (int)harness->get_sp() << std::dec << std::endl;
            }
            state_match = false;
        }
        
        if (harness->get_a() != test->final.a) {
            if (!quiet_mode) {
                debug_output << "FAIL " << test->name << ": A - expected 0x" << std::hex
                            << (int)test->final.a << ", got 0x" << (int)harness->get_a() << std::dec << std::endl;
            }
            state_match = false;
        }
        
        if (harness->get_x() != test->final.x) {
            if (!quiet_mode) {
                debug_output << "FAIL " << test->name << ": X - expected 0x" << std::hex
                            << (int)test->final.x << ", got 0x" << (int)harness->get_x() << std::dec << std::endl;
            }
            state_match = false;
        }
        
        if (harness->get_y() != test->final.y) {
            if (!quiet_mode) {
                debug_output << "FAIL " << test->name << ": Y - expected 0x" << std::hex
                            << (int)test->final.y << ", got 0x" << (int)harness->get_y() << std::dec << std::endl;
            }
            state_match = false;
        }
        
        if (harness->get_status() != test->final.p) {
            if (!quiet_mode) {
                debug_output << "FAIL " << test->name << ": P - expected 0x" << std::hex
                            << (int)test->final.p << ", got 0x" << (int)harness->get_status() << std::dec << std::endl;
            }
            state_match = false;
        }
        
        // Memory state comparison
        for (uint8_t i = 0; i < test->final.ram_count; i++) {
            uint16_t addr = test->final.ram[i].address;
            for (uint8_t j = 0; j < test->final.ram[i].byte_count; j++) {
                uint8_t expected_value = test->final.ram[i].bytes[j];
                uint8_t actual_value = harness->get_memory(addr + j);
                
                if (actual_value != expected_value) {
                    if (!quiet_mode) {
                        debug_output << "FAIL " << test->name << ": Memory[0x" << std::hex << (addr + j)
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
                debug_output << "FAIL " << test->name << ": Cycles - expected " << test->final.cycles
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
                output << debug_output.str(); // Emit debug output only if verbose
                output << "PASS " << test->name << " (opcode 0x" << std::hex
                       << (int)current_opcode << ")" << std::dec << std::endl;
            }
            
            // REMOVED: Clear global harness (thread safety issue)
            // current_test_harness = nullptr;
            return true;
        } else {
            results.failed_tests++;
            if (!state_match) results.state_mismatches++;
            results.record_opcode_result(current_opcode, false); // Record failure
            
            // Always emit debug output on failure (unless quiet mode)
            if (!quiet_mode) {
                output << debug_output.str();
                output << "FAIL " << test->name << ": ";
                if (!state_match) output << "State ";
                if (!cycle_match) output << "Cycle ";
                if (!bus_cycle_match) output << "BusCycle ";
                output << "mismatch (opcode 0x" << std::hex << (int)current_opcode << ")" << std::dec << std::endl;
            }
            
            // REMOVED: Clear global harness (thread safety issue)
            // current_test_harness = nullptr;
            return false;
        }
    }
};

// Global variables
bool verbose_output = false;
static TestResults results;

// Parallel file processing - reuses pre-allocated vector for performance
void collect_tests_from_file(const std::string& filepath, std::vector<TestItem>& tests) {
    tests.clear(); // Clear previous contents but keep allocated memory
    
    // Optimize: Use memory-mapped file reading for better performance
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "ERROR: Could not open file: " << filepath << std::endl;
        return; // Early return, tests vector remains empty
    }
    
    // Get file size and read in one go
    std::streamsize size = file.tellg();
    if (size <= 0) {
        return; // Early return, tests vector remains empty
    }
    
    file.seekg(0, std::ios::beg);
    std::string json_content(size, '\0');
    if (!file.read(json_content.data(), size)) {
        std::cout << "ERROR: Could not read file: " << filepath << std::endl;
        return; // Early return, tests vector remains empty
    }
    file.close();
    
    // Fastest possible approach for large JSON arrays
    tests.reserve(10000); // Pre-allocate for typical test count
    
    if (json_content[0] == '[') {
        // Use raw pointers for maximum performance on large data
        const char* data = json_content.data();
        const char* end = data + json_content.size();
        const char* pos = data + 1; // Skip opening [
        
        size_t test_index = 0;
        size_t safety_limit = 1000000; // Prevent infinite loops
        
        while (pos < end && safety_limit-- > 0) {
            // Fast search for opening brace using memchr
            pos = static_cast<const char*>(memchr(pos, '{', end - pos));
            if (!pos) break;
            
            // Fast brace matching using pointer arithmetic
            const char* obj_start = pos;
            int depth = 1;
            ++pos; // Skip opening brace
            
            while (pos < end && depth > 0) {
                if (*pos == '{') ++depth;
                else if (*pos == '}') --depth;
                ++pos;
            }
            
            if (depth == 0) {
                // Use emplace_back to construct in-place
                tests.emplace_back();
                TestItem& item = tests.back();
                item.filepath = filepath;
                item.test_json.assign(obj_start, pos - obj_start);
                item.test_name = "test_" + std::to_string(test_index++);
            }
        }
    } else {
        // Single test - still create locally since it's only one
        TestItem item;
        item.filepath = filepath;
        item.test_json = std::move(json_content);
        item.test_name = "single_test";
        tests.push_back(std::move(item));
    }
}

// Optimized directory processing for parallel execution
std::vector<TestItem> collect_all_tests(const std::vector<std::string>& test_paths) {
    std::vector<TestItem> all_tests;
    
    // First pass: count all JSON files to reserve space
    size_t total_files = 0;
    std::vector<std::string> json_files;
    
    for (const auto& test_path : test_paths) {
        try {
            if (fs::is_directory(test_path)) {
                // Use iterative approach for better performance
                std::error_code ec;
                for (const auto& entry : fs::recursive_directory_iterator(test_path, ec)) {
                    if (ec) {
                        std::cout << "WARNING: Error accessing " << entry.path() << ": " << ec.message() << std::endl;
                        continue;
                    }
                    
                    if (entry.is_regular_file(ec) && !ec && entry.path().extension() == ".json") {
                        json_files.push_back(entry.path().string());
                        total_files++;
                    }
                }
            } else if (fs::is_regular_file(test_path)) {
                json_files.push_back(test_path);
                total_files++;
            } else {
                std::cout << "ERROR: Invalid path: " << test_path << std::endl;
            }
        } catch (const fs::filesystem_error& ex) {
            std::cout << "ERROR: Could not access path: " << test_path 
                      << " (" << ex.what() << ")" << std::endl;
        }
    }
    
    // Reserve space for better performance (expect 10000 tests per file)
    all_tests.reserve(total_files * 10000);
    
    // Pre-allocate reusable vector for file processing (avoid repeated allocations)
    std::vector<TestItem> file_tests;
    // Reserve space for 10000 tests (typical per file) for better performance
    file_tests.reserve(10000);
    
    // Second pass: actually collect tests using reusable vector
    for (const auto& json_file : json_files) {
        collect_tests_from_file(json_file, file_tests);
        all_tests.insert(all_tests.end(), 
                        std::make_move_iterator(file_tests.begin()),
                        std::make_move_iterator(file_tests.end()));
    }
    
    return all_tests;
}

// Enhanced usage information
void print_usage(const char* program_name) {
    std::cout << "fam65xx ProcessorTests Runner - Multi-Processor Edition\n";
    std::cout << "Usage: " << program_name << " [options] <test_file_or_directory>\n";
    std::cout << "\nProcessor Selection:\n";
    std::cout << "  -p, --processor P  Specify processor type (overrides auto-detection)\n";
    std::cout << "                     Supported: mos6502, nes6502, mos6510, wdc65c02, rockwell65c02, wdc65c816\n";
    std::cout << "                     Aliases: 6502, nes, 6510, 65c02, rockwell, 65c816\n";
    std::cout << "\nTest Execution Options:\n";
    std::cout << "  -v, --verbose      Enable verbose output with detailed execution logs\n";
    std::cout << "  -q, --quiet        Quiet mode - only show final summary\n";
    std::cout << "  -c, --continue     Continue testing after failures (default: stop on first failure)\n";
    std::cout << "  -s, --stop-first   Stop on first failure (default behavior)\n";
    std::cout << "  -j, --jobs N       Number of parallel jobs (default: CPU cores - 1)\n";
    std::cout << "  -h, --help         Show this help message\n";
    std::cout << "\nProcessor Auto-Detection:\n";
    std::cout << "  If no -p flag is specified, processor type is auto-detected from test path:\n";
    std::cout << "  • processor_tests/6502/v1/      → MOS 6502\n";
    std::cout << "  • processor_tests/nes6502/v1/   → NES 6502 (Ricoh 2A03/2A07)\n";
    std::cout << "  • processor_tests/mos6510/v1/   → MOS 6510 (C64)\n";
    std::cout << "  • processor_tests/wdc65c02/v1/  → WDC 65C02\n";
    std::cout << "  • processor_tests/rockwell65c02/v1/ → Rockwell 65C02\n";
    std::cout << "  • processor_tests/wdc65c816/v1/ → WDC 65C816\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " processor_tests/6502/v1/                  # Auto-detect MOS 6502\n";
    std::cout << "  " << program_name << " -p nes6502 processor_tests/6502/v1/       # Force NES 6502 on 6502 tests\n";
    std::cout << "  " << program_name << " -j 4 -v processor_tests/nes6502/v1/       # Auto-detect NES, 4 workers, verbose\n";
    std::cout << "  " << program_name << " -p wdc65c02 -q -c processor_tests/        # Force WDC 65C02, quiet mode\n";
    std::cout << "\nFeatures:\n";
    std::cout << "  ✓ Multi-processor support (6502 family)\n";
    std::cout << "  ✓ Automatic processor detection from test path\n";
    std::cout << "  ✓ Manual processor override via command line\n";
    std::cout << "  ✓ Parallel test execution for maximum performance\n";
    std::cout << "  ✓ Thread-safe output (no mixed stdout)\n";
    std::cout << "  ✓ Intelligent core usage (hardware cores - 1)\n";
    std::cout << "  ✓ Hardware-accurate timing validation\n";
}

// Enhanced results printing
void print_results(std::chrono::milliseconds duration, size_t num_workers, ProcessorType processor_type, size_t tests_collected) {
    std::cout << "\n=== FAM65XX PROCESSOR TESTS RESULTS (Multi-Processor Edition) ===\n";
    std::cout << "CPU Implementation: " << get_processor_name(processor_type) << "\n";
    std::cout << "Execution time: " << duration.count() << " ms\n";
    std::cout << "Worker threads: " << num_workers << "\n";
    
    // Show detailed test statistics with context
    std::cout << "Tests collected: " << tests_collected << " (from JSON files)\n";
    std::cout << "Tests executed: " << results.total_tests << "\n";
    
    // Calculate expected total for full processor
    uint32_t expected_total_tests = 256 * 10000;  // 256 opcodes × 10,000 tests each
    
    // Show early termination status
    if (results.total_tests < tests_collected) {
        std::cout << "⚠️  EARLY TERMINATION: Stopped after first failure (--stop-first mode)\n";
        std::cout << "   Remaining tests: " << (tests_collected - results.total_tests) << " not executed\n";
        if (tests_collected < expected_total_tests) {
            std::cout << "   Note: Only " << (tests_collected / 10000) << " of 256 opcodes loaded (" 
                      << std::fixed << std::setprecision(1) 
                      << (double)tests_collected / expected_total_tests * 100.0 << "% of full test suite)\n";
        }
    } else if (tests_collected < expected_total_tests) {
        std::cout << "ℹ️  PARTIAL TEST SUITE: " << (tests_collected / 10000) << " of 256 opcodes (" 
                  << std::fixed << std::setprecision(1) 
                  << (double)tests_collected / expected_total_tests * 100.0 << "% of full processor coverage)\n";
    } else {
        std::cout << "✅ COMPLETE TEST SUITE: All 256 opcodes (full processor coverage)\n";
    }
    
    std::cout << "Tests passed: " << results.passed_tests << "\n";
    std::cout << "Tests failed: " << results.failed_tests << "\n";
    
    if (results.total_tests > 0) {
        double pass_rate = (double)results.passed_tests / results.total_tests * 100.0;
        std::cout << "Pass rate: " << std::fixed << std::setprecision(2) << pass_rate << "% (of executed tests)\n";
        
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
    ProcessorType processor_type_override = ProcessorType::MOS6502;  // Default fallback
    bool processor_specified = false;
    
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
        } else if (arg == "-p" || arg == "--processor") {
            if (i + 1 < argc) {
                try {
                    processor_type_override = parse_processor_type(argv[++i]);
                    processor_specified = true;
                } catch (const std::invalid_argument& e) {
                    std::cout << "ERROR: " << e.what() << "\n";
                    print_usage(argv[0]);
                    return 1;
                }
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
    
    // Auto-detect processor type from first test path if not specified
    ProcessorType detected_processor_type = processor_specified ? 
        processor_type_override : detect_processor_from_path(test_paths[0]);
    
    std::cout << "=== fam65xx ProcessorTests Runner - Multi-Processor Edition ===\n";
    std::cout << "CPU Implementation: " << get_processor_name(detected_processor_type) << "\n";
    if (processor_specified) {
        std::cout << "Processor selection: Manual override (--processor)\n";
    } else {
        std::cout << "Processor selection: Auto-detected from test path\n";
    }
    std::cout << "Test paths: " << test_paths.size() << " specified\n";
    std::cout << "Worker threads: " << num_workers << "\n";
    std::cout << "Verbose: " << (verbose_output ? "enabled" : "disabled") << "\n";
    std::cout << "Quiet mode: " << (g_quiet_mode ? "enabled" : "disabled") << "\n";
    std::cout << "Stop on failure: " << (g_stop_on_failure ? "enabled" : "disabled") << "\n\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Collect all tests first with timing
    std::cout << "Collecting tests..." << std::flush;
    auto collect_start = std::chrono::high_resolution_clock::now();
    auto all_tests = collect_all_tests(test_paths);
    auto collect_end = std::chrono::high_resolution_clock::now();
    auto collect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(collect_end - collect_start);
    std::cout << " Found " << all_tests.size() << " tests in " << collect_duration.count() << "ms\n";
    
    if (all_tests.empty()) {
        std::cout << "No tests found in specified paths!\n";
        return 1;
    }
    
    // Add debug output to identify exact hang location
    std::cout << "DEBUG: About to create ThreadSafeOutput...\n" << std::flush;
    
    // Set up parallel execution
    ThreadSafeOutput output_handler;
    std::cout << "DEBUG: ThreadSafeOutput created\n" << std::flush;
    
    ThreadSafeTestResults thread_results;
    std::cout << "DEBUG: ThreadSafeTestResults created\n" << std::flush;
    
    std::cout << "DEBUG: About to create TestWorkerPool with " << num_workers << " workers...\n" << std::flush;
    TestWorkerPool worker_pool(num_workers, output_handler, thread_results,
                               verbose_output, g_quiet_mode, g_test_failed, g_stop_on_failure,
                               detected_processor_type);
    std::cout << "DEBUG: TestWorkerPool created successfully\n" << std::flush;
    
    // Submit all tests to worker pool
    std::cout << "Starting parallel execution...\n";
    for (const auto& test : all_tests) {
        worker_pool.add_test(test);
    }
    
    // Use proper wait completion method with periodic output flushing
    std::atomic<bool> flush_thread_should_exit{false};
    std::thread flush_thread([&output_handler, &flush_thread_should_exit]() {
        while (!flush_thread_should_exit.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (output_handler.has_pending()) {
                output_handler.flush_all();
            }
        }
    });
    
    // Wait for all tests to complete
    worker_pool.wait_completion();
    
    // Stop the flush thread
    flush_thread_should_exit = true;
    if (flush_thread.joinable()) {
        flush_thread.join();
    }
    
    // Final output flush
    output_handler.flush_all();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Transfer results to global structure
    thread_results.merge_into_global(results);
    
    print_results(duration, num_workers, detected_processor_type, all_tests.size());
    
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