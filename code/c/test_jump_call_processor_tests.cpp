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
#include "tests/json_parser.h"
}

#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include "src/core/system_lines.h"

namespace fs = std::filesystem;

// Simple test harness class for ProcessorTests
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
    
    
    // Execute one instruction using cycle_tick
    bool step() {
        try {
            // Execute cycles until we complete an instruction
            uint32_t initial_cycles = cpu.get_cycle_count();
            uint16_t initial_pc = cpu.get_pc();
            uint8_t initial_step = cpu.get_cycle_step();
            
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
                
                // Check if instruction completed (cycle_step wrapped back to 0)
                if (cpu.get_cycle_step() == 0 && max_cycles > 0) {
                    break;
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

static TestResults results;

// Run a single ProcessorTests test case
bool run_processor_test(const processor_test_t* test) {
    results.total_tests++;
    
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
    
    // Execute one instruction
    uint32_t initial_cycle_count = harness.get_cycle_count();
    bool step_result = harness.step();
    uint32_t cycles_executed = harness.get_cycle_count() - initial_cycle_count;
    
    if (!step_result) {
        results.opcode_failures[opcode]++;
        return false;
    }
    
    // Compare CPU state
    bool passed = true;
    
    if (harness.get_pc() != test->final.pc) {
        passed = false;
    }
    if (harness.get_sp() != test->final.s) {
        passed = false;
    }
    if (harness.get_a() != test->final.a) {
        passed = false;
    }
    if (harness.get_x() != test->final.x) {
        passed = false;
    }
    if (harness.get_y() != test->final.y) {
        passed = false;
    }
    if (harness.get_status() != test->final.p) {
        passed = false;
    }
    
    // Compare memory state
    for (uint8_t i = 0; i < test->final.ram_count; i++) {
        uint16_t addr = test->final.ram[i].address;
        for (uint8_t j = 0; j < test->final.ram[i].byte_count; j++) {
            uint8_t expected_value = test->final.ram[i].bytes[j];
            uint8_t actual_value = harness.get_memory(addr + j);
            
            if (actual_value != expected_value) {
                passed = false;
            }
        }
    }
    
    // Check cycle count if provided
    if (test->final.has_cycles && cycles_executed != test->final.cycles) {
        passed = false;
    }
    
    if (passed) {
        results.passed_tests++;
    } else {
        results.opcode_failures[opcode]++;
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

int main() {
    std::cout << "=== Jump/Call Instructions ProcessorTests Validation ===\n";
    
    // Test jump/call instructions - these are the key opcodes we want to verify
    std::vector<std::string> jump_call_opcodes = {
        "20",  // JSR $nnnn
        "4c",  // JMP $nnnn  
        "6c",  // JMP ($nnnn)
        "40",  // RTI
        "60"   // RTS
    };
    
    for (const auto& opcode : jump_call_opcodes) {
        std::string test_path = "tests/processor_tests/6502/v1/" + opcode + ".json";
        
        std::cout << "Testing opcode 0x" << opcode << " from " << test_path << std::endl;
        
        // Reset results for this opcode
        results.total_tests = 0;
        results.passed_tests = 0;
        std::fill(results.opcode_failures, results.opcode_failures + 256, 0);
        std::fill(results.opcode_totals, results.opcode_totals + 256, 0);
        
        if (run_tests_from_file(test_path)) {
            std::cout << "  Results: " << results.passed_tests << "/" << results.total_tests 
                      << " passed (" << std::fixed << std::setprecision(1) 
                      << (100.0 * results.passed_tests / results.total_tests) << "%)\n";
        } else {
            std::cout << "  ERROR: Could not run tests from " << test_path << std::endl;
        }
    }
    
    std::cout << "\n=== Jump/Call Instructions Test Complete ===\n";
    return 0;
}