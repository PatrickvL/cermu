#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <random>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <bitset>

extern "C" {
#include "tests/json_parser.h"
}

#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include "src/chip/cpu/fam65xx_cpp/cycle_tables.hpp"
#include "src/core/system_lines.h"

namespace fs = std::filesystem;
using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx_with_cycle_count<TestConfig>;

// Performance-optimized test result tracking
struct TestResult {
    uint32_t total = 0;
    uint32_t passed = 0;
    inline double pass_rate() const { return total ? (100.0 * passed / total) : 0.0; }
};

// Consolidated test harness combining all debug program functionality
class UnifiedTestHarness {
private:
    TestCPU cpu;
    uint8_t memory[65536];
    std::map<uint8_t, TestResult> results_by_opcode;
    uint32_t global_test_count = 0;
    uint32_t global_passed_count = 0;
    
    // Inline helper functions for performance optimization
    inline void clear_memory() { std::fill(memory, memory + 65536, 0); }
    inline void setup_stack_data(uint16_t addr, uint8_t data) { memory[addr] = data; }
    inline bool check_flag(uint8_t status, uint8_t flag) const { return (status & flag) != 0; }
    inline uint8_t get_flag_value(uint8_t status, uint8_t flag) const { return (status & flag) ? 1 : 0; }
    
public:
    UnifiedTestHarness() {
        clear_memory();
        cpu.init_for_test();
    }
    
    // Optimized CPU state management
    inline void set_cpu_state(uint16_t pc, uint8_t a, uint8_t x, uint8_t y, uint8_t sp, uint8_t p) {
        cpu.set_pc(pc);
        cpu.set_a(a);
        cpu.set_x(x);
        cpu.set_y(y);
        cpu.set_sp(sp);
        cpu.set_status(p);
    }
    
    inline void reset_cpu_state() {
        cpu.init_for_test();
        clear_memory();
    }
    
    // Memory management with inline optimization
    inline void set_memory_block(uint16_t addr, const uint8_t* data, size_t size) {
        memcpy(&memory[addr], data, size);
    }
    
    inline void set_memory(uint16_t addr, uint8_t data) { memory[addr] = data; }
    inline void set_program(uint16_t addr, const std::vector<uint8_t>& program) {
        for (size_t i = 0; i < program.size(); i++) {
            memory[addr + i] = program[i];
        }
    }
    
    // CPU state reading (optimized accessors)
    inline uint16_t get_pc() const { return cpu.get_pc(); }
    inline uint8_t get_a() const { return cpu.get_a(); }
    inline uint8_t get_x() const { return cpu.get_x(); }
    inline uint8_t get_y() const { return cpu.get_y(); }
    inline uint8_t get_sp() const { return cpu.get_sp(); }
    inline uint8_t get_status() const { return cpu.get_status(); }
    inline uint8_t get_memory(uint16_t addr) const { return memory[addr]; }
    inline uint8_t get_cycle_step() const { return cpu.get_cycle_step(); }
    inline uint32_t get_cycle_count() const { return cpu.get_cycle_count(); }
    
    // High-performance instruction execution with detailed cycle tracking
    inline bool execute_instruction_verbose(uint32_t max_cycles = 10, bool show_cycles = false) {
        try {
            uint32_t initial_cycles = cpu.get_cycle_count();
            
            for (uint32_t cycle = 0; cycle < max_cycles; cycle++) {
                uint16_t addr = cpu.get_address();
                bool is_write = !cpu.get_rw();
                
                if (show_cycles) {
                    std::cout << "Cycle " << (cycle + 1) << ": Addr=0x" << std::hex << std::setfill('0')
                              << std::setw(4) << addr << ", " << (is_write ? "WRITE" : "READ") << std::dec << std::endl;
                }
                
                bus_state_t bus_state = 0;
                if (is_write) {
                    uint8_t data = cpu.get_write_data();
                    memory[addr] = data;
                    bus_state = BUS_SET_DATA(bus_state, data);
                    if (show_cycles) {
                        std::cout << "  Write data: 0x" << std::hex << std::setw(2) << (int)data << std::dec << std::endl;
                    }
                } else {
                    bus_state = BUS_SET_DATA(bus_state, memory[addr]);
                    if (show_cycles) {
                        std::cout << "  Read data: 0x" << std::hex << std::setw(2) << (int)memory[addr] << std::dec << std::endl;
                    }
                }
                
                // Set control lines (optimized)
                bus_state |= BUS_BIT(BUS_RDY_BIT) | BUS_BIT(BUS_SO_BIT);
                
                bus_state = cpu.cycle_tick(bus_state);
                
                // Check completion
                if (cpu.get_cycle_step() == 0 && cycle > 0) {
                    if (show_cycles) {
                        std::cout << "Instruction completed after " << (cycle + 1) << " cycles" << std::endl;
                    }
                    return true;
                }
            }
            return false;
        } catch (...) {
            return false;
        }
    }
    
    // Standard optimized execution
    inline bool execute_instruction(uint32_t max_cycles = 10) {
        return execute_instruction_verbose(max_cycles, false);
    }
    
    // ===== PROCESSORTESTS INTEGRATION (consolidates fam65xx_cpp_processor_tests_runner.cpp) =====
    
    // Run ProcessorTests test case with detailed tracking
    bool run_processor_test(const processor_test_t* test, bool verbose = false) {
        global_test_count++;
        
        // Clear memory
        clear_memory();
        
        // Setup memory from initial state
        for (uint8_t i = 0; i < test->initial.ram_count; i++) {
            uint16_t addr = test->initial.ram[i].address;
            for (uint8_t j = 0; j < test->initial.ram[i].byte_count; j++) {
                memory[addr + j] = test->initial.ram[i].bytes[j];
            }
        }
        
        // Set initial CPU state
        set_cpu_state(test->initial.pc, test->initial.a, test->initial.x,
                     test->initial.y, test->initial.s, test->initial.p);
        
        // Get opcode for tracking
        uint8_t opcode = memory[test->initial.pc];
        results_by_opcode[opcode].total++;
        
        // Execute instruction
        uint32_t initial_cycles = cpu.get_cycle_count();
        if (!execute_instruction()) {
            if (verbose) {
                std::cout << "FAIL " << test->name << ": Instruction execution failed (opcode 0x"
                          << std::hex << (int)opcode << ")" << std::dec << std::endl;
            }
            return false;
        }
        uint32_t cycles_executed = cpu.get_cycle_count() - initial_cycles;
        
        // Compare final state with detailed analysis
        bool passed = true;
        if (get_pc() != test->final.pc) {
            if (verbose) {
                std::cout << "FAIL " << test->name << ": PC - expected 0x" << std::hex
                          << test->final.pc << ", got 0x" << get_pc() << std::dec << std::endl;
            }
            passed = false;
        }
        if (get_sp() != test->final.s) {
            if (verbose) {
                std::cout << "FAIL " << test->name << ": SP - expected 0x" << std::hex
                          << (int)test->final.s << ", got 0x" << (int)get_sp() << std::dec << std::endl;
            }
            passed = false;
        }
        if (get_a() != test->final.a) {
            if (verbose) {
                std::cout << "FAIL " << test->name << ": A - expected 0x" << std::hex
                          << (int)test->final.a << ", got 0x" << (int)get_a() << std::dec << std::endl;
            }
            passed = false;
        }
        if (get_x() != test->final.x) {
            if (verbose) {
                std::cout << "FAIL " << test->name << ": X - expected 0x" << std::hex
                          << (int)test->final.x << ", got 0x" << (int)get_x() << std::dec << std::endl;
            }
            passed = false;
        }
        if (get_y() != test->final.y) {
            if (verbose) {
                std::cout << "FAIL " << test->name << ": Y - expected 0x" << std::hex
                          << (int)test->final.y << ", got 0x" << (int)get_y() << std::dec << std::endl;
            }
            passed = false;
        }
        if (get_status() != test->final.p) {
            if (verbose) {
                std::cout << "FAIL " << test->name << ": P - expected 0x" << std::hex
                          << (int)test->final.p << ", got 0x" << (int)get_status() << std::dec << std::endl;
                print_flag_analysis(test->final.p, get_status());
            }
            passed = false;
        }
        
        // Compare memory state
        for (uint8_t i = 0; i < test->final.ram_count; i++) {
            uint16_t addr = test->final.ram[i].address;
            for (uint8_t j = 0; j < test->final.ram[i].byte_count; j++) {
                if (memory[addr + j] != test->final.ram[i].bytes[j]) {
                    if (verbose) {
                        std::cout << "FAIL " << test->name << ": Memory[0x" << std::hex << (addr + j)
                                  << "] - expected 0x" << (int)test->final.ram[i].bytes[j]
                                  << ", got 0x" << (int)memory[addr + j] << std::dec << std::endl;
                    }
                    passed = false;
                    break;
                }
            }
            if (!passed) break;
        }
        
        // Check cycle count if provided
        if (test->final.has_cycles && cycles_executed != test->final.cycles) {
            if (verbose) {
                std::cout << "FAIL " << test->name << ": Cycles - expected " << test->final.cycles
                          << ", got " << cycles_executed << std::endl;
            }
            passed = false;
        }
        
        if (passed) {
            results_by_opcode[opcode].passed++;
            global_passed_count++;
        }
        
        return passed;
    }
    
    // ===== INSTRUCTION-SPECIFIC TESTING (consolidates debug_* programs) =====
    
    // Test BRK instruction with edge cases (consolidates debug_brk_*)
    void test_brk_instruction() {
        std::cout << "\n=== BRK INSTRUCTION TESTING ===" << std::endl;
        
        struct BrkTest {
            uint8_t initial_p;
            uint16_t initial_pc;
            uint8_t initial_sp;
            const char* description;
        };
        
        std::vector<BrkTest> tests = {
            {0x20, 0x8000, 0xFD, "BRK with U flag set"},
            {0x24, 0x8000, 0xFD, "BRK with I flag set"},
            {0x30, 0x8000, 0xFD, "BRK with B and U flags"},
            {0x00, 0x8000, 0xFD, "BRK with no flags set"},
        };
        
        for (const auto& test : tests) {
            reset_cpu_state();
            
            // Setup BRK instruction
            memory[test.initial_pc] = 0x00; // BRK
            memory[0xFFFE] = 0x00;          // IRQ vector low
            memory[0xFFFF] = 0x80;          // IRQ vector high
            
            set_cpu_state(test.initial_pc, 0x42, 0x33, 0x44, test.initial_sp, test.initial_p);
            
            std::cout << "Test: " << test.description << std::endl;
            std::cout << "Before: PC=0x" << std::hex << test.initial_pc
                      << " SP=0x" << (int)test.initial_sp << " P=0x" << (int)test.initial_p << std::dec << std::endl;
            
            bool success = execute_instruction_verbose(10, false);
            
            std::cout << "After:  PC=0x" << std::hex << get_pc()
                      << " SP=0x" << (int)get_sp() << " P=0x" << (int)get_status() << std::dec << std::endl;
            std::cout << "Stack: [0x" << std::hex << (0x100 + test.initial_sp) << "]=0x" << (int)memory[0x100 + test.initial_sp]
                      << " [0x" << (0x100 + test.initial_sp - 1) << "]=0x" << (int)memory[0x100 + test.initial_sp - 1]
                      << " [0x" << (0x100 + test.initial_sp - 2) << "]=0x" << (int)memory[0x100 + test.initial_sp - 2] << std::dec << std::endl;
            std::cout << "Result: " << (success ? "SUCCESS" : "FAILED") << std::endl << std::endl;
        }
    }
    
    // Test RTI instruction with flag handling (consolidates debug_rti_*)
    void test_rti_instruction() {
        std::cout << "\n=== RTI INSTRUCTION TESTING ===" << std::endl;
        
        struct RtiTest {
            uint8_t stack_p;
            uint8_t stack_pcl;
            uint8_t stack_pch;
            const char* description;
        };
        
        std::vector<RtiTest> tests = {
            {0x34, 0xAA, 0x65, "RTI with B flag (should be cleared)"},
            {0x24, 0xBB, 0x87, "RTI with U flag preservation"},
            {0xFF, 0x00, 0x80, "RTI with all flags set"},
            {0x20, 0x34, 0x12, "RTI with minimal flags"},
        };
        
        for (const auto& test : tests) {
            reset_cpu_state();
            
            // Setup RTI instruction and stack
            memory[0x8000] = 0x40; // RTI
            memory[0x01FE] = test.stack_p;   // Status on stack
            memory[0x01FF] = test.stack_pcl; // PC low
            memory[0x0100] = test.stack_pch; // PC high
            
            set_cpu_state(0x8000, 0x42, 0x33, 0x44, 0xFD, 0x04);
            
            std::cout << "Test: " << test.description << std::endl;
            std::cout << "Before: Stack P=0x" << std::hex << (int)test.stack_p
                      << " PC=0x" << (int)test.stack_pch << (int)test.stack_pcl << std::dec << std::endl;
            
            bool success = execute_instruction_verbose(10, false);
            
            std::cout << "After:  PC=0x" << std::hex << get_pc()
                      << " SP=0x" << (int)get_sp() << " P=0x" << (int)get_status() << std::dec << std::endl;
            std::cout << "Result: " << (success ? "SUCCESS" : "FAILED") << std::endl << std::endl;
        }
    }
    
    // Test shift/rotate operations (consolidates test_all_shift_rotate_opcodes.cpp)
    void test_shift_rotate_operations() {
        std::cout << "\n=== SHIFT/ROTATE OPERATIONS TESTING ===" << std::endl;
        
        struct ShiftTest {
            uint8_t opcode;
            const char* mnemonic;
            uint8_t setup_data;
            uint8_t init_carry;
            uint8_t expect_result;
            uint8_t expect_carry;
            bool expect_n_flag;
            bool expect_z_flag;
        };
        
        std::vector<ShiftTest> tests = {
            // ASL A
            {0x0A, "ASL A", 0x40, 0, 0x80, 0, true, false},
            {0x0A, "ASL A", 0x80, 0, 0x00, 1, false, true},
            // LSR A
            {0x4A, "LSR A", 0x81, 0, 0x40, 1, false, false},
            {0x4A, "LSR A", 0x01, 0, 0x00, 1, false, true},
            // ROL A
            {0x2A, "ROL A", 0x80, 0, 0x00, 1, false, true},
            {0x2A, "ROL A", 0x40, 1, 0x81, 0, true, false},
            // ROR A
            {0x6A, "ROR A", 0x01, 0, 0x00, 1, false, true},
            {0x6A, "ROR A", 0x02, 1, 0x81, 0, true, false},
        };
        
        int passed = 0;
        for (const auto& test : tests) {
            reset_cpu_state();
            
            memory[0x0000] = test.opcode;
            set_cpu_state(0x0000, test.setup_data, 0, 0, 0xFF, test.init_carry ? 0x01 : 0x00);
            
            bool success = execute_instruction();
            
            bool test_passed = success &&
                              (get_a() == test.expect_result) &&
                              (get_flag_value(get_status(), 0x01) == test.expect_carry) &&
                              (check_flag(get_status(), 0x80) == test.expect_n_flag) &&
                              (check_flag(get_status(), 0x02) == test.expect_z_flag);
            
            std::cout << test.mnemonic << ": 0x" << std::hex << (int)test.setup_data
                      << " -> 0x" << (int)get_a() << std::dec
                      << (test_passed ? " ✅" : " ❌") << std::endl;
            
            if (test_passed) passed++;
        }
        
        std::cout << "Shift/Rotate: " << passed << "/" << tests.size() << " passed" << std::endl;
    }
    
    // Test cycle table analysis (consolidates debug_cycle_table.cpp)
    void analyze_cycle_table(uint8_t opcode) {
        std::cout << "\n=== CYCLE TABLE ANALYSIS for 0x" << std::hex << (int)opcode << " ===" << std::dec << std::endl;
        
        for (int step = 1; step <= 8; step++) {
            auto cycle = fam65xx_cpp::CycleTables<TestConfig>::get_cycle(opcode, step);
            std::cout << "Step " << step << ": MemOp=" << (int)cycle.mem_op
                      << " DataOp=" << (int)cycle.data_op
                      << " AluOp=" << (int)cycle.alu_op << std::endl;
            
            // Break if we hit a cycle with all zeros (end of instruction)
            if (cycle.mem_op == 0 && cycle.data_op == 0 && cycle.alu_op == 0) {
                break;
            }
        }
    }
    
    // Print detailed flag analysis (consolidates debug flag analysis)
    inline void print_flag_analysis(uint8_t expected, uint8_t actual) {
        std::cout << "  Flag analysis: Expected=0x" << std::hex << (int)expected
                  << " Actual=0x" << (int)actual << " Diff=0x" << (int)(actual ^ expected) << std::dec << std::endl;
        std::cout << "  N=" << get_flag_value(actual, 0x80) << " (exp=" << get_flag_value(expected, 0x80) << ")"
                  << " V=" << get_flag_value(actual, 0x40) << " (exp=" << get_flag_value(expected, 0x40) << ")"
                  << " U=" << get_flag_value(actual, 0x20) << " (exp=" << get_flag_value(expected, 0x20) << ")"
                  << " B=" << get_flag_value(actual, 0x10) << " (exp=" << get_flag_value(expected, 0x10) << ")" << std::endl
                  << "  D=" << get_flag_value(actual, 0x08) << " (exp=" << get_flag_value(expected, 0x08) << ")"
                  << " I=" << get_flag_value(actual, 0x04) << " (exp=" << get_flag_value(expected, 0x04) << ")"
                  << " Z=" << get_flag_value(actual, 0x02) << " (exp=" << get_flag_value(expected, 0x02) << ")"
                  << " C=" << get_flag_value(actual, 0x01) << " (exp=" << get_flag_value(expected, 0x01) << ")" << std::endl;
    }
    
    // Single opcode test with detailed output
    bool run_single_test(uint8_t opcode, uint16_t pc, uint8_t a, uint8_t x, uint8_t y,
                        uint8_t sp, uint8_t p, const std::vector<uint8_t>& program, bool verbose = false) {
        clear_memory();
        set_program(pc, program);
        set_cpu_state(pc, a, x, y, sp, p);
        
        if (verbose) {
            std::cout << "Testing opcode 0x" << std::hex << (int)opcode << std::dec << std::endl;
            std::cout << "Before: PC=0x" << std::hex << pc << " A=0x" << (int)a
                      << " X=0x" << (int)x << " Y=0x" << (int)y
                      << " SP=0x" << (int)sp << " P=0x" << (int)p << std::dec << std::endl;
        }
        
        bool success = execute_instruction_verbose(10, verbose);
        
        if (verbose) {
            std::cout << "After:  PC=0x" << std::hex << get_pc() << " A=0x" << (int)get_a()
                      << " X=0x" << (int)get_x() << " Y=0x" << (int)get_y()
                      << " SP=0x" << (int)get_sp() << " P=0x" << (int)get_status() << std::dec << std::endl;
            std::cout << "Result: " << (success ? "SUCCESS" : "FAILED") << std::endl;
        }
        
        return success;
    }
    
    // Debug failing test case with comprehensive analysis
    void debug_test_case(const processor_test_t* test, bool verbose = true) {
        if (!verbose) return;
        
        clear_memory();
        for (uint8_t i = 0; i < test->initial.ram_count; i++) {
            uint16_t addr = test->initial.ram[i].address;
            for (uint8_t j = 0; j < test->initial.ram[i].byte_count; j++) {
                memory[addr + j] = test->initial.ram[i].bytes[j];
            }
        }
        set_cpu_state(test->initial.pc, test->initial.a, test->initial.x,
                     test->initial.y, test->initial.s, test->initial.p);
        
        uint8_t opcode = memory[test->initial.pc];
        
        std::cout << "\n=== DEBUG: " << test->name << " (Opcode 0x"
                  << std::hex << (int)opcode << ")" << std::dec << " ===" << std::endl;
        std::cout << "Initial: PC=0x" << std::hex << test->initial.pc
                  << " A=0x" << (int)test->initial.a << " X=0x" << (int)test->initial.x
                  << " Y=0x" << (int)test->initial.y << " SP=0x" << (int)test->initial.s
                  << " P=0x" << (int)test->initial.p << std::dec << std::endl;
        
        // Show cycle table
        analyze_cycle_table(opcode);
        
        // Execute with detailed tracing
        uint32_t initial_cycles = cpu.get_cycle_count();
        bool success = execute_instruction_verbose(10, true);
        uint32_t cycles_executed = cpu.get_cycle_count() - initial_cycles;
        
        std::cout << "Final:   PC=0x" << std::hex << get_pc()
                  << " A=0x" << (int)get_a() << " X=0x" << (int)get_x()
                  << " Y=0x" << (int)get_y() << " SP=0x" << (int)get_sp()
                  << " P=0x" << (int)get_status() << std::dec << std::endl;
        std::cout << "Expected:PC=0x" << std::hex << test->final.pc
                  << " A=0x" << (int)test->final.a << " X=0x" << (int)test->final.x
                  << " Y=0x" << (int)test->final.y << " SP=0x" << (int)test->final.s
                  << " P=0x" << (int)test->final.p << std::dec << std::endl;
        
        if (get_status() != test->final.p) {
            print_flag_analysis(test->final.p, get_status());
        }
        
        std::cout << "Cycles: " << cycles_executed;
        if (test->final.has_cycles) {
            std::cout << " (expected " << test->final.cycles << ")";
        }
        std::cout << std::endl;
        std::cout << "Result: " << (success ? "EXECUTED" : "FAILED") << std::endl;
    }
    
    // Print comprehensive test results
    void print_results() const {
        uint32_t total_passed = 0, total_tests = 0;
        uint32_t failing_opcodes = 0;
        
        std::cout << "\n=== DETAILED OPCODE RESULTS ===" << std::endl;
        for (const auto& [opcode, result] : results_by_opcode) {
            total_passed += result.passed;
            total_tests += result.total;
            
            if (result.passed != result.total) {
                failing_opcodes++;
                std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2)
                          << (int)opcode << std::dec << ": " << result.passed << "/"
                          << result.total << " (" << std::fixed << std::setprecision(1)
                          << result.pass_rate() << "%)" << std::endl;
            }
        }
        
        std::cout << "\n=== COMPREHENSIVE SUMMARY ===" << std::endl;
        std::cout << "Total tests: " << total_tests << std::endl;
        std::cout << "Total passed: " << total_passed << std::endl;
        std::cout << "Overall pass rate: " << std::fixed << std::setprecision(1)
                  << (total_tests ? 100.0 * total_passed / total_tests : 0.0) << "%" << std::endl;
        std::cout << "Failing opcodes: " << failing_opcodes << std::endl;
        std::cout << "Global test count: " << global_test_count << std::endl;
        std::cout << "Global passed count: " << global_passed_count << std::endl;
    }
    
    // Get opcode failure statistics
    std::vector<std::pair<uint8_t, TestResult>> get_failing_opcodes() const {
        std::vector<std::pair<uint8_t, TestResult>> failing;
        for (const auto& [opcode, result] : results_by_opcode) {
            if (result.passed != result.total) {
                failing.push_back({opcode, result});
            }
        }
        return failing;
    }
    
    // Get summary statistics
    inline uint32_t get_total_tests() const { return global_test_count; }
    inline uint32_t get_passed_tests() const { return global_passed_count; }
    inline double get_pass_rate() const { return global_test_count ? (100.0 * global_passed_count / global_test_count) : 0.0; }
};

// ===== CONSOLIDATED TEST FUNCTIONS (replacing all debug_* and test_* programs) =====

// Test critical failing opcodes (replaces all critical debug programs)
void test_critical_failures() {
    UnifiedTestHarness harness;
    
    std::cout << "\n=== CRITICAL FAILING OPCODES ANALYSIS ===" << std::endl;
    std::cout << "Testing opcodes with 100% failure rates in ProcessorTests\n" << std::endl;
    
    // Critical system instructions
    std::cout << "=== SYSTEM INSTRUCTIONS ===" << std::endl;
    harness.test_brk_instruction();
    
    std::vector<uint8_t> php_program = {0x08}; // PHP
    std::cout << "\n--- PHP (0x08) Testing ---" << std::endl;
    harness.run_single_test(0x08, 0x8000, 0x42, 0x33, 0x44, 0xFD, 0x24, php_program, true);
    
    std::vector<uint8_t> plp_program = {0x28}; // PLP
    std::cout << "\n--- PLP (0x28) Testing ---" << std::endl;
    // Setup stack with test data
    harness.set_memory(0x01FE, 0x34);
    harness.run_single_test(0x28, 0x8000, 0x42, 0x33, 0x44, 0xFD, 0x24, plp_program, true);
    
    harness.test_rti_instruction();
    
    // Test shift/rotate operations
    harness.test_shift_rotate_operations();
    
    std::cout << "\nCritical failure analysis completed." << std::endl;
}

// Run ProcessorTests from files or directories (replaces fam65xx_cpp_processor_tests_runner.cpp)
bool run_tests_from_file(UnifiedTestHarness& harness, const std::string& filepath, bool verbose) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "ERROR: Could not open file: " << filepath << std::endl;
        return false;
    }
    
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
                    bool passed = harness.run_processor_test(&test, verbose);
                    if (!passed && verbose) {
                        harness.debug_test_case(&test);
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
            bool passed = harness.run_processor_test(&test, verbose);
            if (!passed && verbose) {
                harness.debug_test_case(&test);
            }
        } else {
            std::cout << "ERROR: Failed to parse test in file: " << filepath << std::endl;
        }
    }
    
    return true;
}

void run_tests_from_directory(UnifiedTestHarness& harness, const std::string& dirpath, bool verbose) {
    try {
        std::cout << "Processing directory: " << dirpath << std::endl;
        
        std::vector<std::string> json_files;
        for (const auto& entry : fs::recursive_directory_iterator(dirpath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                json_files.push_back(entry.path().string());
            }
        }
        
        std::sort(json_files.begin(), json_files.end());
        
        for (const auto& file : json_files) {
            if (verbose) {
                std::cout << "Processing file: " << file << std::endl;
            }
            run_tests_from_file(harness, file, verbose);
        }
    } catch (const fs::filesystem_error& ex) {
        std::cout << "ERROR: Could not process directory: " << dirpath
                  << " (" << ex.what() << ")" << std::endl;
    }
}

// Run comprehensive ProcessorTests validation
int run_processor_tests(const std::vector<std::string>& test_paths, bool verbose = false) {
    std::cout << "=== UNIFIED PROCESSORTESTS RUNNER ===" << std::endl;
    std::cout << "Hardware-verified test validation with consolidated test framework" << std::endl;
    std::cout << "Test paths: " << test_paths.size() << " specified" << std::endl;
    std::cout << "Verbose: " << (verbose ? "enabled" : "disabled") << std::endl << std::endl;
    
    UnifiedTestHarness harness;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Process all test paths
    for (const auto& test_path : test_paths) {
        std::cout << "Processing: " << test_path << std::endl;
        
        try {
            if (fs::is_directory(test_path)) {
                run_tests_from_directory(harness, test_path, verbose);
            } else if (fs::is_regular_file(test_path)) {
                run_tests_from_file(harness, test_path, verbose);
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
    
    std::cout << "\n=== UNIFIED TEST RESULTS ===" << std::endl;
    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;
    harness.print_results();
    
    // Print failure breakdown
    auto failing_opcodes = harness.get_failing_opcodes();
    if (!failing_opcodes.empty()) {
        std::cout << "\n=== FAILURE BREAKDOWN ===\n";
        for (const auto& [opcode, result] : failing_opcodes) {
            std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) << (int)opcode
                      << ": " << std::dec << result.total - result.passed << "/" << result.total << " failed\n";
        }
    }
    
    if (harness.get_total_tests() == 0) {
        std::cout << "\nNo tests found!\n";
        return 1;
    } else if (harness.get_passed_tests() == harness.get_total_tests()) {
        std::cout << "\n✅ ALL TESTS PASSED - 100% ProcessorTests compatibility!\n";
        return 0;
    } else {
        std::cout << "\n❌ TESTS FAILED - Pass rate: " << std::fixed << std::setprecision(1)
                  << harness.get_pass_rate() << "%\n";
        return 1;
    }
}

// Debug specific opcodes (consolidates all debug_* programs)
void debug_specific_opcode(uint8_t opcode, bool verbose = true) {
    UnifiedTestHarness harness;
    
    std::cout << "\n=== DEBUGGING OPCODE 0x" << std::hex << (int)opcode << " ===" << std::dec << std::endl;
    
    // Show cycle table analysis
    harness.analyze_cycle_table(opcode);
    
    // Run basic test cases for the opcode
    std::vector<uint8_t> program = {opcode};
    
    // Test with various register states
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, const char*>> test_states = {
        {0x00, 0x00, 0x00, 0x00, "All zeros"},
        {0xFF, 0xFF, 0xFF, 0x20, "All ones (with U flag)"},
        {0x80, 0x40, 0x20, 0x10, "Mixed values 1"},
        {0x55, 0xAA, 0x33, 0xCC, "Mixed values 2"},
    };
    
    for (const auto& [a, x, y, p, desc] : test_states) {
        std::cout << "\n--- Test: " << desc << " ---" << std::endl;
        bool success = harness.run_single_test(opcode, 0x8000, a, x, y, 0xFD, p, program, verbose);
        std::cout << "Result: " << (success ? "SUCCESS" : "FAILED") << std::endl;
    }
}

// Performance benchmarking (consolidates performance analysis programs)
void run_performance_benchmark() {
    UnifiedTestHarness harness;
    
    std::cout << "\n=== PERFORMANCE BENCHMARK ===" << std::endl;
    
    const int iterations = 10000;
    std::vector<uint8_t> test_program = {0xEA}; // NOP
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        harness.reset_cpu_state();
        harness.run_single_test(0xEA, 0x8000, 0x42, 0x33, 0x44, 0xFD, 0x20, test_program, false);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "Executed " << iterations << " NOP instructions in " << duration.count() << " μs" << std::endl;
    std::cout << "Performance: " << std::fixed << std::setprecision(2)
              << (double)iterations / duration.count() << " million instructions/second" << std::endl;
}

// Print comprehensive usage information
void print_usage(const char* program_name) {
    std::cout << "=== UNIFIED PROCESSOR TEST FRAMEWORK ===" << std::endl;
    std::cout << "Single consolidated program replacing 168+ debug/test programs" << std::endl;
    std::cout << "Combines ProcessorTests validation, instruction debugging, and performance testing" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << program_name << " [options] <command> [args...]" << std::endl;
    std::cout << std::endl;
    std::cout << "COMMANDS:" << std::endl;
    std::cout << "  critical                    - Test critical failing opcodes (BRK, PHP, RTI, etc.)" << std::endl;
    std::cout << "  processortests <path>       - Run ProcessorTests from file or directory" << std::endl;
    std::cout << "  debug <opcode_hex>          - Debug specific opcode with detailed analysis" << std::endl;
    std::cout << "  cycle <opcode_hex>          - Show cycle table analysis for opcode" << std::endl;
    std::cout << "  benchmark                   - Run performance benchmark" << std::endl;
    std::cout << "  validate                    - Run comprehensive validation suite" << std::endl;
    std::cout << std::endl;
    std::cout << "OPTIONS:" << std::endl;
    std::cout << "  -v, --verbose              - Enable verbose output with detailed debugging" << std::endl;
    std::cout << "  -h, --help                 - Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "EXAMPLES:" << std::endl;
    std::cout << "  " << program_name << " critical" << std::endl;
    std::cout << "  " << program_name << " -v processortests processor_tests/6502/v1/" << std::endl;
    std::cout << "  " << program_name << " debug 0x00" << std::endl;
    std::cout << "  " << program_name << " cycle 0x4C" << std::endl;
    std::cout << "  " << program_name << " benchmark" << std::endl;
    std::cout << std::endl;
    std::cout << "CONSOLIDATED FUNCTIONALITY:" << std::endl;
    std::cout << "This program replaces the following 168+ separate programs:" << std::endl;
    std::cout << "- fam65xx_cpp_processor_tests_runner.cpp (ProcessorTests validation)" << std::endl;
    std::cout << "- debug_*.cpp programs (instruction-specific debugging)" << std::endl;
    std::cout << "- test_*.cpp programs (opcode testing and validation)" << std::endl;
    std::cout << "- All specialized debug harnesses and test frameworks" << std::endl;
}

// Main function with comprehensive command-line interface
int main(int argc, char* argv[]) {
    bool verbose = false;
    std::vector<std::string> args;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            args.push_back(arg);
        }
    }
    
    if (args.empty()) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string command = args[0];
    
    std::cout << "=== UNIFIED PROCESSOR TEST FRAMEWORK ===" << std::endl;
    std::cout << "Consolidated 6502/6510 CPU testing and debugging" << std::endl;
    std::cout << "Command: " << command << (verbose ? " (verbose)" : "") << std::endl << std::endl;
    
    try {
        if (command == "critical") {
            test_critical_failures();
            
        } else if (command == "processortests" && args.size() > 1) {
            std::vector<std::string> test_paths(args.begin() + 1, args.end());
            return run_processor_tests(test_paths, verbose);
            
        } else if (command == "debug" && args.size() > 1) {
            uint8_t opcode = std::stoul(args[1], nullptr, 16);
            debug_specific_opcode(opcode, verbose);
            
        } else if (command == "cycle" && args.size() > 1) {
            uint8_t opcode = std::stoul(args[1], nullptr, 16);
            UnifiedTestHarness harness;
            harness.analyze_cycle_table(opcode);
            
        } else if (command == "benchmark") {
            run_performance_benchmark();
            
        } else if (command == "validate") {
            std::cout << "Running comprehensive validation suite..." << std::endl;
            test_critical_failures();
            run_performance_benchmark();
            std::cout << "\nValidation suite completed." << std::endl;
            
        } else {
            std::cout << "ERROR: Unknown command or missing arguments: " << command << std::endl;
            std::cout << "Use --help for usage information." << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "ERROR: Unknown exception occurred" << std::endl;
        return 1;
    }
    
    return 0;
}