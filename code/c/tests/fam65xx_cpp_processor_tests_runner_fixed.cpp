#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <bitset>

extern "C" {
#include "json_parser.h"
}

#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include "../src/core/system_lines.h"

namespace fs = std::filesystem;

// Fixed test harness class for ProcessorTests
class ProcessorTestHarness {
private:
    fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];

public:
    ProcessorTestHarness() {
        // Clear memory
        std::fill(memory, memory + 65536, 0);
        
        // CRITICAL FIX: Use test-specific initialization that doesn't set STATE_RESET_PENDING
        // ProcessorTests expect the CPU to be in normal execution mode, ready to execute
        // the instruction at the current PC, not stuck in a reset sequence
        cpu.init_for_test();
    }
    
    // CPU state accessors
    void set_pc(uint16_t pc) { cpu.set_pc(pc); }
    void set_a(uint8_t a) { cpu.set_a(a); }
    void set_x(uint8_t x) { cpu.set_x(x); }
    void set_y(uint8_t y) { cpu.set_y(y); }
    void set_sp(uint8_t sp) { cpu.set_sp(sp); }
    void set_status(uint8_t p) { cpu.set_status(p); }
    
    uint16_t get_pc() const { return cpu.get_pc(); }
    uint8_t get_a() const { return cpu.get_a(); }
    uint8_t get_x() const { return cpu.get_x(); }
    uint8_t get_y() const { return cpu.get_y(); }
    uint8_t get_sp() const { return cpu.get_sp(); }
    uint8_t get_status() const { return cpu.get_status(); }
    
    // Memory access
    void set_memory(uint16_t addr, uint8_t data) { memory[addr] = data; }
    uint8_t get_memory(uint16_t addr) const { return memory[addr]; }
    
    // Cycle counting
    uint32_t get_cycle_count() const { return cpu.get_cycle_count(); }
    void reset_cycle_count() { cpu.reset_cycle_count(); }
    
    
    // FIXED: Execute one instruction using cycle_tick with proper completion detection
    bool step() {
        try {
            // Execute cycles until we complete an instruction
            uint32_t initial_cycles = cpu.get_cycle_count();
            uint16_t initial_pc = cpu.get_pc();
            uint8_t initial_step = cpu.get_cycle_step();
            
            // CRITICAL FIX: Track whether we've executed at least one full instruction cycle
            bool instruction_started = false;
            
            // Execute cycles with bus interface
            for (int max_cycles = 0; max_cycles < 10; max_cycles++) {
                // Create bus state for reading
                uint16_t addr = cpu.get_address();
                bool is_write = !cpu.get_rw();
                
                bus_state_t bus_state = 0;
                if (is_write) {
                    // Write cycle
                    uint8_t data = cpu.get_write_data();
                    memory[addr] = data;
                    BUS_SET_DATA(bus_state, data);
                } else {
                    // Read cycle
                    uint8_t data = memory[addr];
                    BUS_SET_DATA(bus_state, data);
                }
                
                // Set RDY line (ready) and SO pin (inactive high)
                bus_state |= BUS_BIT(BUS_RDY_BIT);
                bus_state |= BUS_BIT(BUS_SO_BIT);  // SO pin inactive (high)
                
                // Execute one CPU cycle
                bus_state = cpu.cycle_tick(bus_state);
                
                // CRITICAL FIX: Proper instruction completion detection
                // An instruction is complete when:
                // 1. We've moved past the initial cycle_step (instruction has started)
                // 2. We're back to cycle_step 0 (ready for next instruction)
                uint8_t current_step = cpu.get_cycle_step();
                
                if (current_step != initial_step) {
                    instruction_started = true;
                }
                
                // FIXED: Only check for completion after instruction has started
                if (instruction_started && current_step == 0) {
                    break;  // Instruction completed - do NOT execute another cycle
                }
            }
            
            return true;
        } catch (...) {
            return false;
        }
    }
};

struct TestResults {
    uint32_t total_tests = 0;
    uint32_t passed_tests = 0;
    uint32_t opcode_failures[256] = {0};
    uint32_t opcode_totals[256] = {0};
};

static bool verbose_output = false;
static TestResults results;

// Run a single ProcessorTests test case
bool run_processor_test(const processor_test_t* test) {
    results.total_tests++;
    
    if (verbose_output) {
        std::cout << "Running test: " << test->name << " on fam65xx_cpp" << std::endl;
    }
    
    // Create test harness
    ProcessorTestHarness harness;
    
    // Setup memory from initial state
    for (uint8_t i = 0; i < test->initial.ram_count; i++) {
        uint16_t addr = test->initial.ram[i].address;
        for (uint8_t j = 0; j < test->initial.ram[i].byte_count; j++) {
            harness.set_memory(addr + j, test->initial.ram[i].bytes[j]);
        }
    }
    
    // Set initial CPU state
    harness.set_pc(test->initial.pc);
    harness.set_a(test->initial.a);
    harness.set_x(test->initial.x);
    harness.set_y(test->initial.y);
    harness.set_sp(test->initial.s);
    harness.set_status(test->initial.p);
    
    
    // Get the opcode for tracking
    uint8_t opcode = harness.get_memory(test->initial.pc);
    results.opcode_totals[opcode]++;
    
    if (verbose_output) {
        std::cout << "  Opcode at PC 0x" << std::hex << test->initial.pc 
                  << ": 0x" << std::hex << (int)opcode << std::dec << std::endl;
    }
    
    // Execute one instruction
    uint32_t initial_cycle_count = harness.get_cycle_count();
    bool step_result = harness.step();
    uint32_t cycles_executed = harness.get_cycle_count() - initial_cycle_count;
    
    if (!step_result) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": Instruction execution failed (opcode 0x" 
                      << std::hex << (int)opcode << ")" << std::dec << std::endl;
        }
        results.opcode_failures[opcode]++;
        return false;
    }
    
    // Compare CPU state
    bool passed = true;
    
    if (harness.get_pc() != test->final.pc) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": PC - expected 0x" << std::hex 
                      << test->final.pc << ", got 0x" << harness.get_pc() << std::dec << std::endl;
        }
        passed = false;
    }
    if (harness.get_sp() != test->final.s) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": SP - expected 0x" << std::hex 
                      << (int)test->final.s << ", got 0x" << (int)harness.get_sp() << std::dec << std::endl;
        }
        passed = false;
    }
    if (harness.get_a() != test->final.a) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": A - expected 0x" << std::hex 
                      << (int)test->final.a << ", got 0x" << (int)harness.get_a() << std::dec << std::endl;
        }
        passed = false;
    }
    if (harness.get_x() != test->final.x) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": X - expected 0x" << std::hex 
                      << (int)test->final.x << ", got 0x" << (int)harness.get_x() << std::dec << std::endl;
        }
        passed = false;
    }
    if (harness.get_y() != test->final.y) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": Y - expected 0x" << std::hex 
                      << (int)test->final.y << ", got 0x" << (int)harness.get_y() << std::dec << std::endl;
        }
        passed = false;
    }
    if (harness.get_status() != test->final.p) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": P - expected 0x" << std::hex
                      << (int)test->final.p << ", got 0x" << (int)harness.get_status() << std::dec << std::endl;
            
            // DEBUG: Add detailed flag analysis
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
        passed = false;
    }
    
    // Compare memory state
    for (uint8_t i = 0; i < test->final.ram_count; i++) {
        uint16_t addr = test->final.ram[i].address;
        for (uint8_t j = 0; j < test->final.ram[i].byte_count; j++) {
            uint8_t expected_value = test->final.ram[i].bytes[j];
            uint8_t actual_value = harness.get_memory(addr + j);
            
            if (actual_value != expected_value) {
                if (verbose_output) {
                    std::cout << "FAIL " << test->name << ": Memory[0x" << std::hex << (addr + j) 
                              << "] - expected 0x" << (int)expected_value 
                              << ", got 0x" << (int)actual_value << std::dec << std::endl;
                }
                passed = false;
            }
        }
    }
    
    // Check cycle count if provided
    if (test->final.has_cycles && cycles_executed != test->final.cycles) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": Cycles - expected " << test->final.cycles 
                      << ", got " << cycles_executed << std::endl;
        }
        passed = false;
    }
    
    if (passed) {
        results.passed_tests++;
        if (verbose_output) {
            std::cout << "PASS " << test->name << " (opcode 0x" << std::hex 
                      << (int)opcode << ")" << std::dec << std::endl;
        }
    } else {
        results.opcode_failures[opcode]++;
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": State mismatch (opcode 0x" 
                      << std::hex << (int)opcode << ")" << std::dec << std::endl;
        }
    }
    
    return passed;
}

bool run_tests_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "ERROR: Could not open file: " << filepath << std::endl;
        return false;
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
                
                // Parse and run the test
                processor_test_t test;
                if (json_parse_processor_test(test_json.c_str(), &test)) {
                    run_processor_test(&test);
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
        } else {
            std::cout << "ERROR: Failed to parse test in file: " << filepath << std::endl;
        }
    }
    
    return true;
}

void run_tests_from_directory(const std::string& dirpath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dirpath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::cout << "Processing file: " << entry.path() << std::endl;
                run_tests_from_file(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error& ex) {
        std::cout << "ERROR: Could not process directory: " << dirpath 
                  << " (" << ex.what() << ")" << std::endl;
    }
}

void print_usage(const char* program_name) {
    std::cout << "FIXED fam65xx_cpp ProcessorTests Runner - Hardware-verified test validation\n";
    std::cout << "Usage: " << program_name << " [options] <test_file_or_directory>\n";
    std::cout << "Options:\n";
    std::cout << "  -v, --verbose    Enable verbose output\n";
    std::cout << "  -h, --help       Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " processor_tests/6502/v1/\n";
    std::cout << "  " << program_name << " -v processor_tests/6502/v1/69.json\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::vector<std::string> test_paths;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose_output = true;
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
    
    std::cout << "=== FIXED fam65xx_cpp ProcessorTests Runner ===\n";
    std::cout << "Test paths: " << test_paths.size() << " specified\n";
    std::cout << "Verbose: " << (verbose_output ? "enabled" : "disabled") << "\n\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Process all test paths
    for (const auto& test_path : test_paths) {
        std::cout << "Processing test path: " << test_path << std::endl;
        
        try {
            if (fs::is_directory(test_path)) {
                run_tests_from_directory(test_path);
            } else if (fs::is_regular_file(test_path)) {
                run_tests_from_file(test_path);
            } else {
                std::cout << "ERROR: Invalid path: " << test_path << std::endl;
            }
        } catch (const fs::filesystem_error& ex) {
            std::cout << "ERROR: Could not access path: " << test_path 
                      << " (" << ex.what() << ")" << std::endl;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\n=== TEST SUMMARY for FIXED fam65xx_cpp ===\n";
    std::cout << "Total tests run: " << results.total_tests << "\n";
    std::cout << "Tests passed: " << results.passed_tests << "\n";
    std::cout << "Tests failed: " << (results.total_tests - results.passed_tests) << "\n";
    std::cout << "Execution time: " << duration.count() << " ms\n";
    
    // Print opcode failure summary
    if (results.total_tests - results.passed_tests > 0) {
        std::cout << "\n=== FAILURE BREAKDOWN BY OPCODE ===\n";
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
    
    if (results.total_tests == 0) {
        std::cout << "\nNo tests found in specified path!\n";
        return 1;
    } else if (results.passed_tests == results.total_tests) {
        std::cout << "\nALL TESTS PASSED - FIXED fam65xx_cpp matches ProcessorTests ground truth!\n";
        return 0;
    } else {
        double pass_rate = (double)results.passed_tests / results.total_tests * 100.0;
        std::cout << "\nSOME TESTS FAILED - FIXED fam65xx_cpp pass rate: " 
                  << std::fixed << std::setprecision(1) << pass_rate << "%\n";
        std::cout << "Implementation differs from hardware-verified ground truth\n";
        return 1;
    }
}