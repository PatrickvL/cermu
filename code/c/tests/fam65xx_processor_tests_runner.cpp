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

#define CHIPS_IMPL
#include "../src/chip/cpu/fam65xx_cpp/opcode_gen/fam65xx.h"

namespace fs = std::filesystem;

// Simple test harness class for ProcessorTests using fam65xx.h
class ProcessorTestHarness {
private:
    fam65xx_t cpu;
    uint8_t memory[65536];
    uint32_t cycle_count;
    
    // Memory callbacks for fam65xx.h
    static uint8_t mem_read(void* user_data, uint16_t addr, uint8_t bus_state) {
        ProcessorTestHarness* harness = static_cast<ProcessorTestHarness*>(user_data);
        return harness->memory[addr];
    }
    
    static void mem_write(void* user_data, uint16_t addr, uint8_t data) {
        ProcessorTestHarness* harness = static_cast<ProcessorTestHarness*>(user_data);
        harness->memory[addr] = data;
    }

public:
    ProcessorTestHarness() : cycle_count(0) {
        // Clear memory
        std::fill(memory, memory + 65536, 0);
        
        // Initialize CPU with memory callbacks
        fam65xx_desc_t desc = {};
        desc.mem_read = mem_read;
        desc.mem_write = mem_write;
        desc.mem_user_data = this;
        
        uint64_t pins = fam65xx_init(&cpu, &desc);
        
        // ProcessorTests expects CPU to be ready for immediate execution
        // No reset sequence - the CPU should be initialized in a state
        // where it can directly execute instructions starting with _FETCH()
        cycle_count = 0;
    }
    
    // CPU state accessors
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
    
    // Memory access
    void set_memory(uint16_t addr, uint8_t data) { memory[addr] = data; }
    uint8_t get_memory(uint16_t addr) const { return memory[addr]; }
    
    // Cycle counting
    uint32_t get_cycle_count() const { return cycle_count; }
    void reset_cycle_count() { cycle_count = 0; }
    
    // Execute one instruction
    bool step() {
        try {
            uint32_t max_cycles = 100; // Safety limit
            
            // Execute cycles until instruction is complete
            uint64_t pins = FAM65XX_RDY; // Set RDY high
            
            do {
                pins = fam65xx_tick(&cpu, pins);
                cycle_count++;
                max_cycles--;
                if (max_cycles == 0) {
                    return false; // Exceeded cycle limit
                }
            } while (!fam65xx_opdone(&cpu));
            
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

bool verbose_output = false;
static TestResults results;

// Run a single ProcessorTests test case
bool run_processor_test(const processor_test_t* test) {
    results.total_tests++;
    
    if (verbose_output) {
        std::cout << "Running test: " << test->name << " on fam65xx.h" << std::endl;
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
    
    // CRITICAL: Set up CPU for fetch at the test PC
    // This ensures the first tick will read the opcode from the correct address
    // and trigger SYNC processing
    
    // Get the opcode for tracking
    uint16_t pc_addr = test->initial.pc;
    uint8_t current_opcode = harness.get_memory(pc_addr);
    results.opcode_totals[current_opcode]++;
    
    if (verbose_output) {
        std::cout << "  Opcode at PC 0x" << std::hex << test->initial.pc
                  << ": 0x" << std::hex << (int)current_opcode << std::dec << std::endl;
    }
    
    // Execute one instruction
    uint32_t initial_cycle_count = harness.get_cycle_count();
    
    if (verbose_output) {
        std::cout << "  DEBUG: About to execute opcode 0x" << std::hex << (int)current_opcode
                  << " at PC 0x" << test->initial.pc << std::dec << std::endl;
    }
    
    bool step_result = harness.step();
    uint32_t cycles_executed = harness.get_cycle_count() - initial_cycle_count;
    
    if (verbose_output) {
        std::cout << "  DEBUG: After execution - PC = 0x" << std::hex << harness.get_pc()
                  << ", SP = 0x" << (int)harness.get_sp() << ", cycles = " << std::dec << cycles_executed << std::endl;
    }
    
    if (!step_result) {
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": Instruction execution failed (opcode 0x"
                      << std::hex << (int)current_opcode << ")" << std::dec << std::endl;
        }
        results.opcode_failures[current_opcode]++;
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
                      << (int)current_opcode << ")" << std::dec << std::endl;
        }
    } else {
        results.opcode_failures[current_opcode]++;
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": State mismatch (opcode 0x"
                      << std::hex << (int)current_opcode << ")" << std::dec << std::endl;
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
    std::cout << "fam65xx.h ProcessorTests Runner - Hardware-verified test validation\n";
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
    
    std::cout << "=== fam65xx.h ProcessorTests Runner ===\n";
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
    
    std::cout << "\n=== TEST SUMMARY for fam65xx.h ===\n";
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
        std::cout << "\nALL TESTS PASSED - fam65xx.h matches ProcessorTests ground truth!\n";
        return 0;
    } else {
        double pass_rate = (double)results.passed_tests / results.total_tests * 100.0;
        std::cout << "\nSOME TESTS FAILED - fam65xx.h pass rate: " 
                  << std::fixed << std::setprecision(1) << pass_rate << "%\n";
        std::cout << "Implementation differs from hardware-verified ground truth\n";
        return 1;
    }
}