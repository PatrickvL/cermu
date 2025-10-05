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
    
    // Performance optimizations - track individual written addresses
    std::vector<uint16_t> written_addresses;  // Track which addresses were written to
    bool memory_tracking_enabled;
    
    // Memory callbacks - reliable approach from C++ version
    static uint8_t mem_read(void* user_data, uint16_t addr, uint8_t bus_state) {
        ProcessorTestHarness* harness = static_cast<ProcessorTestHarness*>(user_data);
        uint8_t value = harness->memory[addr];
        extern bool verbose_output;
        if (verbose_output) {
            std::cout << "    MEM_READ: addr=0x" << std::hex << addr << ", data=0x" << (int)value << std::dec << std::endl;
        }
        return value;
    }
    
    static void mem_write(void* user_data, uint16_t addr, uint8_t data) {
        ProcessorTestHarness* harness = static_cast<ProcessorTestHarness*>(user_data);
        extern bool verbose_output;
        if (verbose_output) {
            std::cout << "    MEM_WRITE: addr=0x" << std::hex << addr << ", data=0x" << (int)data << std::dec << std::endl;
        }
        harness->memory[addr] = data;
        
        // Track individual write for optimization
        harness->track_written_address(addr);
    }

    // Track individual written address for selective clearing
    void track_written_address(uint16_t addr) {
        if (!memory_tracking_enabled) return;
        
        // Check if address is already tracked (avoid duplicates)
        for (uint16_t written_addr : written_addresses) {
            if (written_addr == addr) {
                return; // Already tracked
            }
        }
        
        // Add new written address
        written_addresses.push_back(addr);
    }

    // Bootstrap processor for ProcessorTests compatibility
    void bootstrap_processor_for_tests() {
        // For ProcessorTests: ensure first tick starts with proper fetch setup
        if (cpu.CI == 0xFFFF) {
            // First tick after initialization - set up for instruction fetch
            cpu.AD = cpu.PC;
            // Don't set CI here - let the step function handle the first fetch
        }
    }

public:
    ProcessorTestHarness() : cycle_count(0), memory_tracking_enabled(true) {
        // Clear memory (optimized approach from C version)
        std::fill(memory, memory + 65536, 0);
        
        // Initialize CPU with memory callbacks (reliable C++ approach)
        fam65xx_desc_t desc = {};
        desc.mem_read = mem_read;
        desc.mem_write = mem_write;
        desc.mem_user_data = this;
        
        uint64_t pins = fam65xx_init(&cpu, &desc);
        
        // ProcessorTests expects CPU to be ready for immediate execution
        cycle_count = 0;
    }
    
    // Fast memory clearing - only clear bytes that were actually written
    void clear_written_memory() {
        if (!memory_tracking_enabled) {
            std::fill(memory, memory + 65536, 0);
            return;
        }
        
        // Clear only the addresses that were written to
        for (uint16_t addr : written_addresses) {
            memory[addr] = 0;
        }
        written_addresses.clear();
    }
    
    // Setup memory for new test with optimizations
    void setup_memory_for_test(const cpu_state_t* initial) {
        // Clear only previously written addresses for performance
        clear_written_memory();
        
        // Set up memory from RAM entries and track written addresses
        for (int i = 0; i < initial->ram_count; i++) {
            uint16_t addr = initial->ram[i].address;
            for (int j = 0; j < initial->ram[i].byte_count; j++) {
                memory[addr + j] = initial->ram[i].bytes[j];
                track_written_address(addr + j);
            }
        }
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
    
    // Memory access
    void set_memory(uint16_t addr, uint8_t data) {
        memory[addr] = data;
        track_written_address(addr);
    }
    uint8_t get_memory(uint16_t addr) const { return memory[addr]; }
    
    // Cycle counting
    uint32_t get_cycle_count() const { return cycle_count; }
    void reset_cycle_count() { cycle_count = 0; }
    
    // Execute one instruction - reliable C++ approach
    bool step() {
        try {
            uint32_t max_cycles = 100; // Safety limit
            extern bool verbose_output;
            
            // Execute cycles until instruction is complete
            uint64_t pins = FAM65XX_RDY; // Set RDY high
            
            if (verbose_output) {
                std::cout << "  DEBUG: Starting step execution, initial PC=0x" << std::hex << get_pc() << std::dec << std::endl;
            }
            
            do {
                // For ProcessorTests: ensure first tick starts with proper fetch setup
                if (cpu.CI == 0xFFFF) {
                    // First tick after initialization - set up for instruction fetch
                    cpu.AD = cpu.PC;
                    pins |= FAM65XX_SYNC;  // Set SYNC for instruction fetch
                    pins |= FAM65XX_RW;    // CRITICAL: Ensure RW is set for read operation!
                    cpu.CI = 0x0000;        // Clear the invalid marker
                }
                
                if (verbose_output) {
                    std::cout << "  DEBUG: Before tick " << (cycle_count + 1) << " - PC=0x" << std::hex << get_pc()
                              << ", CI=0x" << cpu.CI << ", RW=" << ((pins & FAM65XX_RW) ? 1 : 0) << std::dec << std::endl;
                }

                pins = fam65xx_tick(&cpu, pins);
                cycle_count++;
                if (verbose_output) {
                    std::cout << "  DEBUG: After tick " << cycle_count << " - PC=0x" << std::hex << get_pc()
                              << ", CI=0x" << cpu.CI << ", RW=" << ((pins & FAM65XX_RW) ? 1 : 0)
                              << ", opdone=" << fam65xx_opdone(&cpu) << std::dec << std::endl;
                }
                max_cycles--;
                if (max_cycles == 0) {
                    return false; // Exceeded cycle limit
                }
            } while (!fam65xx_opdone(&cpu));
            
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
    uint32_t opcode_failures[256] = {0};
    uint32_t opcode_totals[256] = {0};
};

// Global variables with enhanced options (C version features)
bool verbose_output = false;
static bool g_quiet_mode = false;
static bool g_stop_on_failure = true;
static bool g_test_failed = false;
static TestResults results;

// Run a single test with  best practices
bool run__processor_test(const processor_test_t* test) {
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
    
    // Check registers - FAIL messages shown unless in quiet mode (C version feature)
    if (harness.get_pc() != test->final.pc) {
        if (!g_quiet_mode) {
            std::cout << "FAIL " << test->name << ": PC - expected 0x" << std::hex 
                      << test->final.pc << ", got 0x" << harness.get_pc() << std::dec << std::endl;
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
    
    if (state_match && cycle_match) {
        results.passed_tests++;
        if (verbose_output) {
            std::cout << "PASS " << test->name << " (opcode 0x" << std::hex
                      << (int)current_opcode << ")" << std::dec << std::endl;
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
                if (harness.get_pc() != test->final.pc) {
                    std::cout << "  PC: expected 0x" << std::hex << test->final.pc 
                              << ", got 0x" << harness.get_pc() << " (diff: " << std::dec 
                              << ((int)harness.get_pc() - (int)test->final.pc) << ")\n";
                }
            }
            
            if (!cycle_match && test->final.has_cycles) {
                std::cout << "Cycle mismatch:\n";
                std::cout << "  Expected: " << test->final.cycles << " cycles, Got: " 
                          << cycles_executed << " cycles (diff: " 
                          << ((int)cycles_executed - (int)test->final.cycles) << ")\n";
            }
            
            std::cout << "\nUse --continue flag to run through all tests despite failures.\n";
        }
        
        if (verbose_output) {
            std::cout << "FAIL " << test->name << ": State mismatch (opcode 0x"
                      << std::hex << (int)current_opcode << ")" << std::dec << std::endl;
        }
        
        return false;
    }
}

// File processing with enhanced error handling (combined approach)
bool process__test_file(const std::string& filepath) {
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
                    run__processor_test(&test);
                    
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
            run__processor_test(&test);
            
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
void process__directory(const std::string& dirpath) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dirpath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                if (!g_quiet_mode) {
                    std::cout << "Processing file: " << entry.path() << std::endl;
                }
                process__test_file(entry.path().string());
                
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
void print__usage(const char* program_name) {
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
void print__results() {
    std::cout << "\n=== CONSOLIDATED FAM65XX PROCESSOR TESTS RESULTS ===\n";
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

// Main function with  features
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print__usage(argv[0]);
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
            print__usage(argv[0]);
            return 0;
        } else {
            test_paths.push_back(arg);
        }
    }
    
    if (test_paths.empty()) {
        std::cout << "ERROR: No test file or directory specified\n";
        print__usage(argv[0]);
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
                process__directory(test_path);
            } else if (fs::is_regular_file(test_path)) {
                process__test_file(test_path);
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
    print__results();
    
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