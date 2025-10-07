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

extern "C" {
#include "json_parser.h"
}

#define CHIPS_IMPL
#include "../src/chip/cpu/fam65xx_cpp/opcode_gen/fam65xx.h"

namespace fs = std::filesystem;

//  test harness combining C++ reliability with C optimizations
class ProcessorTestHarness {
private:
    fam65xx_t cpu;
    uint8_t memory[65536];
    uint32_t cycle_count;
    uint64_t pins;  // Maintain pins state across steps
    
    // Bus cycle tracking for comparing against JSON test data
    std::vector<bus_cycle_t> actual_bus_cycles;
    
    // Memory callbacks - reliable approach from C++ version
    static uint8_t mem_read(void* user_data, uint16_t addr, uint8_t bus_state) {
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
        
        extern bool verbose_output;
        if (verbose_output) {
            std::cout << "    BUS_CYCLE: " << (is_write ? "WRITE" : "READ")
                      << " addr=0x" << std::hex << addr
                      << ", data=0x" << (int)data << std::dec << std::endl;
        }
    }


public:
    // Bootstrap processor for ProcessorTests compatibility
    void bootstrap_processor_for_tests() {
        // Only bootstrap if not already done
        if (cpu.CI == 0xFFFF) {
            pins = fam65xx_bootstrap(&cpu, pins);
        }
    }

    ProcessorTestHarness() : cycle_count(0) {
        // Clear memory (optimized approach from C version)
        std::fill(memory, memory + 65536, 0);
        
        // Initialize CPU with memory callbacks (reliable C++ approach)
        fam65xx_desc_t desc = {};
        desc.mem_read = mem_read;
        desc.mem_write = mem_write;
        desc.mem_user_data = this;
        
        pins = fam65xx_init(&cpu, &desc);
        
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
    void setup_memory_for_test(const cpu_state_t* initial) {
        // Clear bus cycle tracking for new test
        clear_bus_cycles();
        
        // Set up memory from RAM entries
        for (int i = 0; i < initial->ram_count; i++) {
            uint16_t addr = initial->ram[i].address;
            for (int j = 0; j < initial->ram[i].byte_count; j++) {
                memory[addr + j] = initial->ram[i].bytes[j];
            }
        }
    }
    
    // Clear bus cycle tracking
    void clear_bus_cycles() {
        actual_bus_cycles.clear();
    }
    
    // Get recorded bus cycles
    const std::vector<bus_cycle_t>& get_bus_cycles() const {
        return actual_bus_cycles;
    }
    
    // CPU state accessors (C++ version approach)
    void set_pc(uint16_t pc) { fam65xx_set_pc(&cpu, pc); }
    void set_a(uint8_t a) { fam65xx_set_a(&cpu, a); }
    void set_x(uint8_t x) { fam65xx_set_x(&cpu, x); }
    void set_y(uint8_t y) { fam65xx_set_y(&cpu, y); }
    void set_sp(uint8_t sp) { fam65xx_set_s(&cpu, sp); }
    void set_status(uint8_t p) { fam65xx_set_p(&cpu, p); }
    
    uint16_t get_pc() const { return fam65xx_pc(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t get_a() const { return fam65xx_a(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t get_x() const { return fam65xx_x(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t get_y() const { return fam65xx_y(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t get_sp() const { return fam65xx_s(const_cast<fam65xx_t*>(&cpu)); }
    uint8_t get_status() const { return fam65xx_p(const_cast<fam65xx_t*>(&cpu)); }
    
    // Memory access (for direct memory setup, not during CPU execution)
    void set_memory(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
    uint8_t get_memory(uint16_t addr) const { return memory[addr]; }
    
    // Cycle counting
    uint32_t get_cycle_count() const { return cycle_count; }
    void reset_cycle_count() { cycle_count = 0; }
    
    // Execute one instruction - SYNC-based completion detection
    bool step() {
        try {
            uint32_t max_cycles = 10; // Safety limit
            extern bool verbose_output;
            uint16_t initial_pc = get_pc();
            
            if (verbose_output) {
                std::cout << "  DEBUG: Starting step execution, initial PC=0x" << std::hex << initial_pc << std::dec << std::endl;
            }
            
            do {
                if (verbose_output) {
                    std::cout << "  DEBUG: Before tick " << (cycle_count + 1) << " - PC=0x" << std::hex << get_pc()
                              << ", CI=0x" << cpu.CI << ", RW=" << ((pins & FAM65XX_RW) ? 1 : 0)
                              << ", SYNC=" << ((pins & FAM65XX_SYNC) ? 1 : 0) << std::dec << std::endl;
                }

                pins = fam65xx_tick(&cpu, pins);
                cycle_count++;
                
                // Instruction completes when SYNC is set, indicating fetch_next was called
                bool instruction_done = (pins & FAM65XX_SYNC) != 0;
                
                if (verbose_output) {
                    std::cout << "  DEBUG: After tick " << cycle_count << " - PC=0x" << std::hex << get_pc()
                              << ", CI=0x" << cpu.CI << ", RW=" << ((pins & FAM65XX_RW) ? 1 : 0)
                              << ", SYNC=" << ((pins & FAM65XX_SYNC) ? 1 : 0)
                              << ", opdone=" << (instruction_done ? 1 : 0) << std::dec << std::endl;
                }
                
                max_cycles--;
                if (max_cycles == 0) {
                    return false; // Exceeded cycle limit
                }
                
                if (instruction_done) {
                    break; // Instruction completed - SYNC indicates ready for next instruction
                }
            } while (true);
            
            if (verbose_output) {
                std::cout << "  DEBUG: Step completed after " << (100 - max_cycles) << " cycles" << std::endl;
            }
            
            return true;
        } catch (...) {
            return false;
        }
    }
};

// Test results tracking with enhanced statistics (C version features)
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

// Global variables with enhanced options (C version features)
bool verbose_output = false;
static bool g_quiet_mode = false;
static bool g_stop_on_failure = true;
static bool g_test_failed = false;
static TestResults results;

// Helper function to compare bus cycles
bool compare_bus_cycles(const std::vector<bus_cycle_t>& actual, const cpu_state_t& expected, const std::string& test_name) {
    if (!expected.has_bus_cycles) {
        // No expected bus cycles to compare
        return true;
    }
    
    bool match = true;
    size_t max_cycles = std::max(actual.size(), (size_t)expected.bus_cycle_count);
    
    extern bool verbose_output;
    if (verbose_output) {
        std::cout << "  BUS_CYCLE_COMPARE: Expected " << (int)expected.bus_cycle_count
                  << " cycles, got " << actual.size() << " cycles" << std::endl;
    }
    
    // Check cycle count first
    if (actual.size() != expected.bus_cycle_count) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test_name << ": Bus cycle count - expected "
                      << (int)expected.bus_cycle_count << ", got " << actual.size() << std::endl;
        }
        match = false;
    }
    
    // Compare individual cycles up to the minimum count
    size_t min_cycles = std::min(actual.size(), (size_t)expected.bus_cycle_count);
    for (size_t i = 0; i < min_cycles; i++) {
        const bus_cycle_t& actual_cycle = actual[i];
        const bus_cycle_t& expected_cycle = expected.bus_cycles[i];
        
        bool cycle_match = true;
        
        if (actual_cycle.address != expected_cycle.address) {
            if (!g_quiet_mode) {
                std::cout << "FAIL " << test_name << ": Bus cycle[" << i << "] address - expected 0x"
                          << std::hex << expected_cycle.address << ", got 0x" << actual_cycle.address << std::dec << std::endl;
            }
            cycle_match = false;
        }
        
        if (actual_cycle.data != expected_cycle.data) {
            if (!g_quiet_mode) {
                std::cout << "FAIL " << test_name << ": Bus cycle[" << i << "] data - expected 0x"
                          << std::hex << (int)expected_cycle.data << ", got 0x" << (int)actual_cycle.data << std::dec << std::endl;
            }
            cycle_match = false;
        }
        
        if (actual_cycle.is_write != expected_cycle.is_write) {
            if (!g_quiet_mode) {
                std::cout << "FAIL " << test_name << ": Bus cycle[" << i << "] type - expected "
                          << (expected_cycle.is_write ? "WRITE" : "READ") << ", got "
                          << (actual_cycle.is_write ? "WRITE" : "READ") << std::endl;
            }
            cycle_match = false;
        }
        
        if (verbose_output && cycle_match) {
            std::cout << "  BUS_CYCLE[" << i << "] MATCH: " << (actual_cycle.is_write ? "WRITE" : "READ")
                      << " addr=0x" << std::hex << actual_cycle.address
                      << ", data=0x" << (int)actual_cycle.data << std::dec << std::endl;
        }
        
        if (!cycle_match) {
            match = false;
        }
    }
    
    // Report any extra cycles
    if (actual.size() > expected.bus_cycle_count) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test_name << ": Extra actual bus cycles:" << std::endl;
            for (size_t i = expected.bus_cycle_count; i < actual.size(); i++) {
                const bus_cycle_t& cycle = actual[i];
                std::cout << "  [" << i << "] " << (cycle.is_write ? "WRITE" : "READ")
                          << " addr=0x" << std::hex << cycle.address
                          << ", data=0x" << (int)cycle.data << std::dec << std::endl;
            }
        }
    }
    
    if (expected.bus_cycle_count > actual.size()) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test_name << ": Missing actual bus cycles:" << std::endl;
            for (size_t i = actual.size(); i < expected.bus_cycle_count; i++) {
                const bus_cycle_t& cycle = expected.bus_cycles[i];
                std::cout << "  [" << i << "] " << (cycle.is_write ? "WRITE" : "READ")
                          << " addr=0x" << std::hex << cycle.address
                          << ", data=0x" << (int)cycle.data << std::dec << std::endl;
            }
        }
    }
    
    return match;
}

// Run a single test with best practices
bool run_processor_test(const processor_test_t* test) {
    results.total_tests++;
    
    if (verbose_output) {
        std::cout << "Running test: " << test->name << " on fam65xx.h" << std::endl;
    }
    
    // Create test harness with optimizations
    ProcessorTestHarness harness;
    
    // Setup memory with performance optimizations
    harness.setup_memory_for_test(&test->initial);
    
    // Set initial CPU state (C++ reliable approach)
    harness.set_pc(test->initial.pc);
    harness.set_a(test->initial.a);
    harness.set_x(test->initial.x);
    harness.set_y(test->initial.y);
    harness.set_sp(test->initial.s);
    harness.set_status(test->initial.p);

    // Get the opcode for tracking
    uint16_t pc_addr = test->initial.pc;
    uint8_t current_opcode = harness.get_memory(pc_addr);
    results.opcode_totals[current_opcode]++;
    
    if (verbose_output) {
        std::cout << "  Opcode at PC 0x" << std::hex << test->initial.pc
                  << ": 0x" << std::hex << (int)current_opcode << std::dec << std::endl;
    }
    
    // Execute one instruction (reliable C++ approach)
    uint32_t initial_cycle_count = harness.get_cycle_count();
    
    if (verbose_output) {
        std::cout << "  DEBUG: About to execute opcode 0x" << std::hex << (int)current_opcode
                  << " at PC 0x" << test->initial.pc << std::dec << std::endl;
    }
    
    // Bootstrap processor after PC is set properly
    harness.bootstrap_processor_for_tests();
    
    bool step_result = harness.step();
    uint32_t cycles_executed = harness.get_cycle_count() - initial_cycle_count;
    
    if (verbose_output) {
        std::cout << "  DEBUG: After execution - PC = 0x" << std::hex << harness.get_pc()
                  << ", SP = 0x" << (int)harness.get_sp() << ", cycles = " << std::dec << cycles_executed << std::endl;
    }
    
    if (!step_result) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": Instruction execution failed (opcode 0x"
                      << std::hex << (int)current_opcode << ")" << std::dec << std::endl;
        }
        results.failed_tests++;
        results.opcode_failures[current_opcode]++;
        return false;
    }
    
    // Compare CPU state with enhanced reporting
    bool state_match = true;
    bool cycle_match = true;
    bool bus_cycle_match = true;
    
    // Check registers - FAIL messages shown unless in quiet mode (C version feature)
    // Note: PC is incremented by fetch_next, so subtract 1 when comparing to ProcessorTests expectation
    uint16_t actual_pc = harness.get_pc() - 1;
    if (actual_pc != test->final.pc) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": PC - expected 0x" << std::hex
                      << test->final.pc << ", got 0x" << actual_pc << std::dec << std::endl;
        }
        state_match = false;
    }
    if (harness.get_sp() != test->final.s) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": SP - expected 0x" << std::hex 
                      << (int)test->final.s << ", got 0x" << (int)harness.get_sp() << std::dec << std::endl;
        }
        state_match = false;
    }
    if (harness.get_a() != test->final.a) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": A - expected 0x" << std::hex 
                      << (int)test->final.a << ", got 0x" << (int)harness.get_a() << std::dec << std::endl;
        }
        state_match = false;
    }
    if (harness.get_x() != test->final.x) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": X - expected 0x" << std::hex 
                      << (int)test->final.x << ", got 0x" << (int)harness.get_x() << std::dec << std::endl;
        }
        state_match = false;
    }
    if (harness.get_y() != test->final.y) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": Y - expected 0x" << std::hex 
                      << (int)test->final.y << ", got 0x" << (int)harness.get_y() << std::dec << std::endl;
        }
        state_match = false;
    }
    if (harness.get_status() != test->final.p) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": P - expected 0x" << std::hex
                      << (int)test->final.p << ", got 0x" << (int)harness.get_status() << std::dec << std::endl;
            
            // DEBUG: Add detailed flag analysis (C++ version feature)
            if (verbose_output) {
                uint8_t expected = test->final.p;
                uint8_t actual = harness.get_status();
                std::cout << "  DEBUG: Expected P=0x" << std::hex << (int)expected << std::endl;
                std::cout << "  DEBUG: Actual P=0x" << std::hex << (int)actual << std::endl;
                std::cout << "  DEBUG: Difference=0x" << std::hex << (int)(actual ^ expected) << std::endl;
                
                // Flag breakdown
                std::cout << "  DEBUG: N=" << ((actual & 0x80) ? 1 : 0) << " (exp=" << ((expected & 0x80) ? 1 : 0) << ")" << std::endl;
                std::cout << "  DEBUG: V=" << ((actual & 0x40) ? 1 : 0) << " (exp=" << ((expected & 0x40) ? 1 : 0) << ")" << std::endl;
                std::cout << "  DEBUG: U=" << ((actual & 0x20) ? 1 : 0) << " (exp=" << ((expected & 0x20) ? 1 : 0) << ")" << std::endl;
                std::cout << "  DEBUG: B=" << ((actual & 0x10) ? 1 : 0) << " (exp=" << ((expected & 0x10) ? 1 : 0) << ")" << std::endl;
                std::cout << "  DEBUG: D=" << ((actual & 0x08) ? 1 : 0) << " (exp=" << ((expected & 0x08) ? 1 : 0) << ")" << std::endl;
                std::cout << "  DEBUG: I=" << ((actual & 0x04) ? 1 : 0) << " (exp=" << ((expected & 0x04) ? 1 : 0) << ")" << std::endl;
                std::cout << "  DEBUG: Z=" << ((actual & 0x02) ? 1 : 0) << " (exp=" << ((expected & 0x02) ? 1 : 0) << ")" << std::endl;
                std::cout << "  DEBUG: C=" << ((actual & 0x01) ? 1 : 0) << " (exp=" << ((expected & 0x01) ? 1 : 0) << ")" << std::endl;
            }
        }
        state_match = false;
    }
    
    // Compare memory state
    for (uint8_t i = 0; i < test->final.ram_count; i++) {
        uint16_t addr = test->final.ram[i].address;
        for (uint8_t j = 0; j < test->final.ram[i].byte_count; j++) {
            uint8_t expected_value = test->final.ram[i].bytes[j];
            uint8_t actual_value = harness.get_memory(addr + j);
            
            if (actual_value != expected_value) {
                if (!g_quiet_mode) {
                    std::cout << "FAIL " << test->name << ": Memory[0x" << std::hex << (addr + j) 
                              << "] - expected 0x" << (int)expected_value 
                              << ", got 0x" << (int)actual_value << std::dec << std::endl;
                }
                state_match = false;
            }
        }
    }
    
    // Check cycle count if provided
    if (test->final.has_cycles && cycles_executed != test->final.cycles) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": Cycles - expected " << test->final.cycles 
                      << ", got " << cycles_executed << std::endl;
        }
        cycle_match = false;
        results.cycle_mismatches++;
    }
    
    // Compare bus cycles if available
    extern bool verbose_output;
    if (verbose_output) {
        std::cout << "  DEBUG: About to compare bus cycles, has_bus_cycles=" << (test->final.has_bus_cycles ? "true" : "false")
                  << ", cycle_count=" << (int)test->final.bus_cycle_count << std::endl;
    }
    
    bus_cycle_match = compare_bus_cycles(harness.get_bus_cycles(), test->final, test->name);
    if (!bus_cycle_match) {
        results.bus_cycle_mismatches++;
    }
    
    // Clean up memory after test completion
    harness.clear_written_memory(&test->initial);

    if (state_match && cycle_match && bus_cycle_match) {
        results.passed_tests++;
        if (verbose_output) {
            std::cout << "PASS " << test->name << " (opcode 0x" << std::hex
                      << (int)current_opcode << ")" << std::dec;
            if (test->final.has_bus_cycles) {
                std::cout << " bus_cycles=" << harness.get_bus_cycles().size();
            }
            std::cout << std::endl;
        }
        return true;
    } else {
        results.failed_tests++;
        if (!state_match) results.state_mismatches++;
        results.opcode_failures[current_opcode]++;
        
        // Set global failure flag for stop-on-failure mode
        g_test_failed = true;
        
        // Enhanced failure reporting (C version feature)
        if (g_stop_on_failure && !g_quiet_mode) {
            std::cout << "\n=== FIRST FAILURE DETECTED - STOPPING EXECUTION ===\n";
            std::cout << "Failed test: " << test->name << "\n";
            std::cout << "Opcode: 0x" << std::hex << (int)current_opcode << std::dec << "\n";
            
            if (!state_match) {
                std::cout << "State mismatches detected:\n";
                uint16_t actual_pc = harness.get_pc() - 1;
                if (actual_pc != test->final.pc) {
                    std::cout << "  PC: expected 0x" << std::hex << test->final.pc
                              << ", got 0x" << actual_pc << " (diff: " << std::dec
                              << ((int)actual_pc - (int)test->final.pc) << ")\n";
                }
            }
            
            if (!cycle_match && test->final.has_cycles) {
                std::cout << "Cycle mismatch:\n";
                std::cout << "  Expected: " << test->final.cycles << " cycles, Got: " 
                          << cycles_executed << " cycles (diff: "
                          << ((int)cycles_executed - (int)test->final.cycles) << ")\n";
            }
            
            if (!bus_cycle_match && test->final.has_bus_cycles) {
                std::cout << "Bus cycle mismatch:\n";
                std::cout << "  Expected: " << (int)test->final.bus_cycle_count
                          << " bus cycles, Got: " << harness.get_bus_cycles().size()
                          << " bus cycles\n";
            }
            
            std::cout << "\nUse --continue flag to run through all tests despite failures.\n";
        }
        
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": ";
            if (!state_match) std::cout << "State ";
            if (!cycle_match) std::cout << "Cycle ";
            if (!bus_cycle_match) std::cout << "BusCycle ";
            std::cout << "mismatch (opcode 0x" << std::hex << (int)current_opcode << ")" << std::dec << std::endl;
        }
        
        
        return false;
    }
    
    return true;
}


// File processing with enhanced error handling (combined approach)
bool process_test_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "ERROR: Could not open file: " << filepath << std::endl;
        return false;
    }
    
    // Read entire file
    std::string json_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    // Parse JSON - handle both single tests and arrays (C++ approach)
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
                
                // Parse and run the test
                processor_test_t test;
                if (json_parse_processor_test(test_json.c_str(), &test)) {
                    run_processor_test(&test);
                    
                    // Check if we should stop on failure (C version feature)
                    if (g_stop_on_failure && g_test_failed) {
                        return false;
                    }
                } else {
                    std::cout << "ERROR: Failed to parse test in file: " << filepath << std::endl;
                }
                
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
        processor_test_t test;
        if (json_parse_processor_test(json_content.c_str(), &test)) {
            run_processor_test(&test);
            
            // Check if we should stop on failure
            if (g_stop_on_failure && g_test_failed) {
                return false;
            }
        } else {
            std::cout << "ERROR: Failed to parse test in file: " << filepath << std::endl;
        }
    }
    
    return true;
}

// Directory processing with enhanced features
void process_directory(const std::string& dirpath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dirpath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                if (!g_quiet_mode) {
                    std::cout << "Processing file: " << entry.path() << std::endl;
                }
                process_test_file(entry.path().string());
                
                // Check if we should stop on failure
                if (g_stop_on_failure && g_test_failed) {
                    return;
                }
            }
        }
    } catch (const fs::filesystem_error& ex) {
        std::cout << "ERROR: Could not process directory: " << dirpath 
                  << " (" << ex.what() << ")" << std::endl;
    }
}

// Enhanced usage information (C version features)
void print_usage(const char* program_name) {
    std::cout << "fam65xx ProcessorTests Runner - Hardware-verified test validation\n";
    std::cout << "Usage: " << program_name << " [options] <test_file_or_directory>\n";
    std::cout << "\nTest Execution Options:\n";
    std::cout << "  -v, --verbose      Enable verbose output with detailed execution logs\n";
    std::cout << "  -q, --quiet        Quiet mode - only show final summary (no individual test failures)\n";
    std::cout << "  -c, --continue     Continue testing after failures (default: stop on first failure)\n";
    std::cout << "  -s, --stop-first   Stop on first failure (default behavior)\n";
    std::cout << "  -h, --help         Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " processor_tests/6502/v1/                    # Run all tests in directory\n";
    std::cout << "  " << program_name << " -v processor_tests/6502/v1/69.json         # Single test with verbose output\n";
    std::cout << "  " << program_name << " -q -c processor_tests/6502/v1/             # Quiet mode, continue on failures\n";
    std::cout << "\nFeatures:\n";
    std::cout << "  ✓ Reliable CPU execution (C++ approach)\n";
    std::cout << "  ✓ Performance optimizations (C approach)\n";
    std::cout << "  ✓ Enhanced error reporting\n";
    std::cout << "  ✓ Memory usage optimization\n";
    std::cout << "  ✓ ProcessorTests JSON compatibility\n";
    std::cout << "  ✓ Hardware-accurate timing validation\n";
}

// Enhanced results printing (C version features)
void print_results() {
    std::cout << "\n=== FAM65XX PROCESSOR TESTS RESULTS ===\n";
    std::cout << "Total tests run: " << results.total_tests << "\n";
    std::cout << "Tests passed: " << results.passed_tests << "\n";
    std::cout << "Tests failed: " << results.failed_tests << "\n";
    
    if (results.total_tests > 0) {
        double pass_rate = (double)results.passed_tests / results.total_tests * 100.0;
        std::cout << "Pass rate: " << std::fixed << std::setprecision(2) << pass_rate << "%\n";
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
        std::cout << "\n🎉 ALL TESTS PASSED - fam65xx is hardware-accurate! 🎉\n";
    } else {
        std::cout << "\n❌ SOME TESTS FAILED - implementation differs from hardware\n";
    }
}

// Main function with features
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::vector<std::string> test_paths;
    
    // Parse command line arguments (enhanced C version approach)
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
    
    std::cout << "=== fam65xx ProcessorTests Runner ===\n";
    std::cout << "Test paths: " << test_paths.size() << " specified\n";
    std::cout << "Verbose: " << (verbose_output ? "enabled" : "disabled") << "\n";
    std::cout << "Quiet mode: " << (g_quiet_mode ? "enabled" : "disabled") << "\n";
    std::cout << "Stop on failure: " << (g_stop_on_failure ? "enabled" : "disabled") << "\n\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Process all test paths
    for (const auto& test_path : test_paths) {
        if (!g_quiet_mode) {
            std::cout << "Processing test path: " << test_path << std::endl;
        }
        
        try {
            if (fs::is_directory(test_path)) {
                process_directory(test_path);
            } else if (fs::is_regular_file(test_path)) {
                process_test_file(test_path);
            } else {
                std::cout << "ERROR: Invalid path: " << test_path << std::endl;
            }
        } catch (const fs::filesystem_error& ex) {
            std::cout << "ERROR: Could not access path: " << test_path 
                      << " (" << ex.what() << ")" << std::endl;
        }
        
        // Check if we should stop on failure
        if (g_stop_on_failure && g_test_failed) {
            break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\nExecution time: " << duration.count() << " ms\n";
    print_results();
    
    if (results.total_tests == 0) {
        std::cout << "\nNo tests found in specified path!\n";
        return 1;
    } else if (results.passed_tests == results.total_tests) {
        std::cout << "\nALL TESTS PASSED - fam65xx matches ProcessorTests ground truth!\n";
        return 0;
    } else {
        double pass_rate = (double)results.passed_tests / results.total_tests * 100.0;
        std::cout << "\nSOME TESTS FAILED - fam65xx pass rate: "
                  << std::fixed << std::setprecision(1) << pass_rate << "%\n";
        std::cout << "Implementation differs from hardware-verified ground truth\n";
        return 1;
    }
}