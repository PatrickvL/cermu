#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>

// Include the CPU implementation
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

// ProcessorTests JSON parsing utilities
extern "C" {
    #include "tests/json_parser.h"
}

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

struct TestResult {
    std::string instruction;
    uint8_t opcode;
    int tests_passed;
    int tests_total;
    bool perfect;
};

// Test a specific set of opcodes and return results
std::vector<TestResult> test_opcodes(const std::vector<uint8_t>& opcodes, const std::string& json_file) {
    std::vector<TestResult> results;
    
    std::ifstream file(json_file);
    if (!file) {
        std::cout << "Cannot open " << json_file << std::endl;
        return results;
    }
    
    // Read entire file
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    for (uint8_t opcode : opcodes) {
        TestResult result;
        result.opcode = opcode;
        result.tests_passed = 0;
        result.tests_total = 0;
        result.perfect = false;
        
        // Parse JSON for this specific opcode
        const char* json_ptr = content.c_str();
        ProcessorTest test;
        
        while (json_find_next_test(json_ptr, &json_ptr, &test)) {
            if (test.initial.opcode == opcode) {
                result.tests_total++;
                
                // Create CPU instance for this test
                TestCPU cpu;
                
                // Set initial state
                cpu.set_reg(fam65xx_cpp::CpuReg::A, test.initial.a);
                cpu.set_reg(fam65xx_cpp::CpuReg::X, test.initial.x);
                cpu.set_reg(fam65xx_cpp::CpuReg::Y, test.initial.y);
                cpu.set_reg(fam65xx_cpp::CpuReg::P, test.initial.p);
                cpu.set_reg(fam65xx_cpp::CpuReg::S, test.initial.s);
                cpu.set_reg(fam65xx_cpp::CpuReg::PC, test.initial.pc);
                
                // Set initial memory
                for (int i = 0; i < test.initial.ram_count; i++) {
                    cpu.write_memory(test.initial.ram[i].address, test.initial.ram[i].value);
                }
                
                // Execute instruction cycles
                int cycle_count = 0;
                while (!cpu.is_sync() && cycle_count < 20) {
                    cpu.step();
                    cycle_count++;
                }
                
                // Compare results
                bool match = true;
                if (cpu.get_reg(fam65xx_cpp::CpuReg::A) != test.final.a) match = false;
                if (cpu.get_reg(fam65xx_cpp::CpuReg::X) != test.final.x) match = false;
                if (cpu.get_reg(fam65xx_cpp::CpuReg::Y) != test.final.y) match = false;
                if (cpu.get_reg(fam65xx_cpp::CpuReg::P) != test.final.p) match = false;
                if (cpu.get_reg(fam65xx_cpp::CpuReg::S) != test.final.s) match = false;
                if (cpu.get_reg(fam65xx_cpp::CpuReg::PC) != test.final.pc) match = false;
                
                // Check memory
                for (int i = 0; i < test.final.ram_count && match; i++) {
                    if (cpu.read_memory(test.final.ram[i].address) != test.final.ram[i].value) {
                        match = false;
                    }
                }
                
                if (match) {
                    result.tests_passed++;
                }
                
                if (result.tests_total >= 100) break; // Limit to first 100 tests for quick validation
            }
        }
        
        result.perfect = (result.tests_passed == result.tests_total) && (result.tests_total > 0);
        
        // Get instruction name for this opcode
        switch (opcode) {
            case 0x8A: result.instruction = "TXA"; break;
            case 0xAA: result.instruction = "TAX"; break;
            case 0x98: result.instruction = "TYA"; break;
            case 0xA8: result.instruction = "TAY"; break;
            case 0xBA: result.instruction = "TSX"; break;
            case 0x9A: result.instruction = "TXS"; break;
            case 0x18: result.instruction = "CLC"; break;
            case 0x38: result.instruction = "SEC"; break;
            case 0x58: result.instruction = "CLI"; break;
            case 0x78: result.instruction = "SEI"; break;
            case 0xB8: result.instruction = "CLV"; break;
            case 0xD8: result.instruction = "CLD"; break;
            case 0xF8: result.instruction = "SED"; break;
            case 0x0A: result.instruction = "ASL A"; break;
            case 0x4A: result.instruction = "LSR A"; break;
            default: 
                result.instruction = "0x" + std::to_string(opcode);
                break;
        }
        
        results.push_back(result);
    }
    
    return results;
}

int main() {
    std::cout << "=== Priority Groups Validation Test ===" << std::endl;
    
    // Priority 1: Transfer Operations
    std::cout << "\n=== PRIORITY 1: Transfer Operations ===" << std::endl;
    std::vector<uint8_t> transfer_ops = {0x8A, 0xAA, 0x98, 0xA8, 0xBA, 0x9A};
    auto transfer_results = test_opcodes(transfer_ops, "tests/6502.json");
    
    int priority1_perfect = 0;
    for (const auto& result : transfer_results) {
        std::cout << std::hex << "0x" << std::setfill('0') << std::setw(2) << (int)result.opcode 
                  << " " << std::setfill(' ') << std::setw(6) << result.instruction 
                  << ": " << std::dec << result.tests_passed << "/" << result.tests_total
                  << (result.perfect ? " ✅" : " ❌") << std::endl;
        if (result.perfect) priority1_perfect++;
    }
    std::cout << "Priority 1 Status: " << priority1_perfect << "/" << transfer_ops.size() << " perfect" << std::endl;
    
    // Priority 2: Flag Operations  
    std::cout << "\n=== PRIORITY 2: Flag Operations ===" << std::endl;
    std::vector<uint8_t> flag_ops = {0x18, 0x38, 0x58, 0x78, 0xB8, 0xD8, 0xF8};
    auto flag_results = test_opcodes(flag_ops, "tests/6502.json");
    
    int priority2_perfect = 0;
    for (const auto& result : flag_results) {
        std::cout << std::hex << "0x" << std::setfill('0') << std::setw(2) << (int)result.opcode 
                  << " " << std::setfill(' ') << std::setw(6) << result.instruction 
                  << ": " << std::dec << result.tests_passed << "/" << result.tests_total
                  << (result.perfect ? " ✅" : " ❌") << std::endl;
        if (result.perfect) priority2_perfect++;
    }
    std::cout << "Priority 2 Status: " << priority2_perfect << "/" << flag_ops.size() << " perfect" << std::endl;
    
    // Priority 4: Accumulator Operations
    std::cout << "\n=== PRIORITY 4: Accumulator Operations ===" << std::endl;
    std::vector<uint8_t> acc_ops = {0x0A, 0x4A};
    auto acc_results = test_opcodes(acc_ops, "tests/6502.json");
    
    int priority4_perfect = 0;
    for (const auto& result : acc_results) {
        std::cout << std::hex << "0x" << std::setfill('0') << std::setw(2) << (int)result.opcode 
                  << " " << std::setfill(' ') << std::setw(6) << result.instruction 
                  << ": " << std::dec << result.tests_passed << "/" << result.tests_total
                  << (result.perfect ? " ✅" : " ❌") << std::endl;
        if (result.perfect) priority4_perfect++;
    }
    std::cout << "Priority 4 Status: " << priority4_perfect << "/" << acc_ops.size() << " perfect" << std::endl;
    
    // Summary
    std::cout << "\n=== VALIDATION SUMMARY ===" << std::endl;
    std::cout << "Priority 1 (Transfer): " << priority1_perfect << "/6 perfect" << std::endl;
    std::cout << "Priority 2 (Flag): " << priority2_perfect << "/7 perfect" << std::endl;
    std::cout << "Priority 4 (Accumulator): " << priority4_perfect << "/2 perfect" << std::endl;
    
    int total_perfect = priority1_perfect + priority2_perfect + priority4_perfect;
    int total_instructions = 6 + 7 + 2;
    std::cout << "TOTAL: " << total_perfect << "/" << total_instructions << " instructions working perfectly" << std::endl;
    
    if (total_perfect == total_instructions) {
        std::cout << "🎉 ALL HIGH-PRIORITY INSTRUCTIONS ARE WORKING PERFECTLY!" << std::endl;
    } else {
        std::cout << "⚠️  Some high-priority instructions still need fixes" << std::endl;
    }
    
    return 0;
}