/**
 * MOS6502 Optimized ProcessorTests Runner
 * 
 * Tests the new optimized MOS6502 emulator against Klaus Dormann's ProcessorTests
 * Hardware-accurate validation using the compact cycle definition architecture
 */

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
#include "../../../../tests/json_parser.h"
}

#include "mos6502_optimized.hpp"
#include "mos6502_cycle_table.hpp"
#include "../../../core/system_lines.h"

namespace fs = std::filesystem;
using namespace fam65xx_cpp;

// ProcessorTests harness for the optimized MOS6502 emulator
class OptimizedProcessorTestHarness {
private:
    using CPU6502 = MOS6502Optimized<config_6502>;
    CPU6502 cpu;
    uint8_t memory[65536];
    uint32_t cycle_count;

public:
    OptimizedProcessorTestHarness() : cycle_count(0) {
        // Clear memory
        std::fill(memory, memory + 65536, 0);
        
        // CPU is already initialized in ready state for testing
        reset_cycle_count();
    }
    
    // CPU state accessors
    void set_pc(uint16_t pc) { cpu.set_pc(pc); }
    void set_a(uint8_t a) { cpu.set_a(a); }
    void set_x(uint8_t x) { cpu.set_x(x); }
    void set_y(uint8_t y) { cpu.set_y(y); }
    void set_sp(uint8_t sp) { cpu.set_sp(sp); }
    void set_status(uint8_t p) { cpu.set_p(p); }
    
    uint16_t get_pc() const { return cpu.get_pc(); }
    uint8_t get_a() const { return cpu.get_a(); }
    uint8_t get_x() const { return cpu.get_x(); }
    uint8_t get_y() const { return cpu.get_y(); }
    uint8_t get_sp() const { return cpu.get_sp(); }
    uint8_t get_status() const { return cpu.get_p(); }
    
    // Memory access
    void set_memory(uint16_t addr, uint8_t data) { memory[addr] = data; }
    uint8_t get_memory(uint16_t addr) const { return memory[addr]; }
    
    // Cycle counting
    uint32_t get_cycle_count() const { return cycle_count; }
    void reset_cycle_count() { cycle_count = 0; }
    
    // Execute one instruction using the optimized φ1/φ2 tick system
    bool step() {
        try {
            uint32_t initial_cycles = cycle_count;
            uint16_t initial_pc = cpu.get_pc();
            uint8_t initial_cycle = cpu.get_current_cycle();
            
            // Execute cycles until instruction completion
            for (int max_cycles = 0; max_cycles < 10; max_cycles++) {
                cycle_count++;
                
                // Create bus state for memory interface
                uint16_t addr = 0;
                uint8_t data = 0;
                bus_state_t bus_state = BUS_STATE(addr, data, BUS_MASK_RW | BUS_MASK_AEC);
                
                // φ1 phase - internal CPU operations
                bus_state = cpu.tick_phi1(bus_state);
                
                // φ2 phase - external bus operations  
                bus_state = cpu.tick_phi2(bus_state);
                
                // Memory access occurs AFTER φ2 completes
                addr = BUS_GET_ADDR(bus_state);
                bool is_write = !(bus_state & BUS_BIT(BUS_RW_BIT));
                
                if (is_write) {
                    // Write cycle
                    data = BUS_GET_DATA(bus_state);
                    memory[addr] = data;
                } else {
                    // Read cycle - provide data to CPU
                    data = memory[addr];
                    bus_state = BUS_SET_DATA(bus_state, data);
                }
                
                // Check if instruction completed (cycle reset to 0)
                if (cpu.get_current_cycle() == 0 && max_cycles > 0) {
                    break;
                }
                
                // Safety check - avoid infinite loops
                if (max_cycles >= 9) {
                    std::cout << "ERROR: Instruction took too many cycles (>10)" << std::endl;
                    return false;
                }
            }
            
            return true;
        } catch (const std::exception& e) {
            std::cout << "ERROR: Exception during instruction execution: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cout << "ERROR: Unknown exception during instruction execution" << std::endl;
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
        std::cout << "Running test: " << test->name << " on MOS6502Optimized" << std::endl;
    }
    
    // Create test harness
    OptimizedProcessorTestHarness harness;
    
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
        std::cout << "  Initial state: A=0x" << std::hex << (int)test->initial.a
                  << " X=0x" << (int)test->initial.x << " Y=0x" << (int)test->initial.y
                  << " P=0x" << (int)test->initial.p << " SP=0x" << (int)test->initial.s
                  << " PC=0x" << test->initial.pc << std::dec << std::endl;
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
            
            // Detailed flag analysis
            uint8_t expected = test->final.p;
            uint8_t actual = harness.get_status();
            std::cout << "  Flag breakdown:" << std::endl;
            std::cout << "    N=" << ((actual & 0x80) ? 1 : 0) << " (exp=" << ((expected & 0x80) ? 1 : 0) << ")" << std::endl;
            std::cout << "    V=" << ((actual & 0x40) ? 1 : 0) << " (exp=" << ((expected & 0x40) ? 1 : 0) << ")" << std::endl;
            std::cout << "    U=" << ((actual & 0x20) ? 1 : 0) << " (exp=" << ((expected & 0x20) ? 1 : 0) << ")" << std::endl;
            std::cout << "    B=" << ((actual & 0x10) ? 1 : 0) << " (exp=" << ((expected & 0x10) ? 1 : 0) << ")" << std::endl;
            std::cout << "    D=" << ((actual & 0x08) ? 1 : 0) << " (exp=" << ((expected & 0x08) ? 1 : 0) << ")" << std::endl;
            std::cout << "    I=" << ((actual & 0x04) ? 1 : 0) << " (exp=" << ((expected & 0x04) ? 1 : 0) << ")" << std::endl;
            std::cout << "    Z=" << ((actual & 0x02) ? 1 : 0) << " (exp=" << ((expected & 0x02) ? 1 : 0) << ")" << std::endl;
            std::cout << "    C=" << ((actual & 0x01) ? 1 : 0) << " (exp=" << ((expected & 0x01) ? 1 : 0) << ")" << std::endl;
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
    std::cout << "MOS6502 Optimized ProcessorTests Runner - Hardware-accurate validation\n";
    std::cout << "Usage: " << program_name << " [options] <test_file_or_directory>\n";
    std::cout << "Options:\n";
    std::cout << "  -v, --verbose    Enable verbose output\n";
    std::cout << "  -h, --help       Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " processor_tests/6502/v1/\n";
    std::cout << "  " << program_name << " -v processor_tests/6502/v1/69.json\n";
    std::cout << "\n";
    std::cout << "This runner tests the new MOS6502Optimized implementation with:\n";
    std::cout << "  • Compact cycle storage through mutually exclusive circuit groups\n";
    std::cout << "  • Hardware-accurate φ1/φ2 separation\n";
    std::cout << "  • Template-driven multi-variant architecture\n";
    std::cout << "  • Complete cycle-perfect timing\n";
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
    
    std::cout << "=== MOS6502 Optimized ProcessorTests Runner ===\n";
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
    
    std::cout << "\n=== TEST SUMMARY for MOS6502Optimized ===\n";
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
        std::cout << "\n🎉 ALL TESTS PASSED - MOS6502Optimized matches ProcessorTests ground truth!\n";
        std::cout << "✅ Hardware-accurate emulation verified\n";
        std::cout << "✅ Compact cycle definitions validated\n";
        std::cout << "✅ Template-driven architecture confirmed\n";
        return 0;
    } else {
        double pass_rate = (double)results.passed_tests / results.total_tests * 100.0;
        std::cout << "\n❌ SOME TESTS FAILED - MOS6502Optimized pass rate: " 
                  << std::fixed << std::setprecision(1) << pass_rate << "%\n";
        std::cout << "Implementation requires fixes to match hardware-verified ground truth\n";
        return 1;
    }
}