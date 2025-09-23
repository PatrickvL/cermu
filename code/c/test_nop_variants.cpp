#include <iostream>
#include <vector>
#include <iomanip>
#include "tests/fam65xx_cpp_test_harness.cpp"

// All NOP variant opcodes (27 instructions)
std::vector<uint8_t> nop_opcodes = {
    // Documented NOP
    0xEA,
    
    // Undocumented NOP variants (single-byte NOPs)
    0x1A, 0x3A, 0x5A, 0x7A, 0xDA, 0xFA,
    
    // Undocumented NOP variants (2-byte NOPs - immediate mode)
    0x80, 0x82, 0x89, 0xC2, 0xE2,
    
    // Undocumented NOP variants (2-byte NOPs - zero page)
    0x04, 0x44, 0x64,
    
    // Undocumented NOP variants (3-byte NOPs - absolute)
    0x0C,
    
    // Undocumented NOP variants (2-byte NOPs - zero page,X)
    0x14, 0x34, 0x54, 0x74, 0xD4, 0xF4,
    
    // Undocumented NOP variants (3-byte NOPs - absolute,X)
    0x1C, 0x3C, 0x5C, 0x7C, 0xDC, 0xFC
};

int main() {
    std::cout << "=== Testing NOP Variants (Priority 7) ===" << std::endl;
    std::cout << "Testing " << nop_opcodes.size() << " NOP variant opcodes..." << std::endl;
    
    int passed = 0;
    int total = 0;
    
    for (uint8_t opcode : nop_opcodes) {
        total++;
        
        try {
            auto cpu = create_cpu();
            auto& bus = get_memory_bus(cpu);
            
            // Set up test case based on addressing mode
            std::vector<uint8_t> program;
            uint16_t initial_pc = 0x1000;
            uint16_t expected_pc;
            
            if (opcode == 0xEA || opcode == 0x1A || opcode == 0x3A || opcode == 0x5A || 
                opcode == 0x7A || opcode == 0xDA || opcode == 0xFA) {
                // Single-byte NOPs
                program = {opcode};
                expected_pc = initial_pc + 1;
            } else if (opcode == 0x80 || opcode == 0x82 || opcode == 0x89 || opcode == 0xC2 || 
                       opcode == 0xE2 || opcode == 0x04 || opcode == 0x44 || opcode == 0x64 ||
                       opcode == 0x14 || opcode == 0x34 || opcode == 0x54 || opcode == 0x74 ||
                       opcode == 0xD4 || opcode == 0xF4) {
                // 2-byte NOPs
                program = {opcode, 0x42};
                expected_pc = initial_pc + 2;
            } else {
                // 3-byte NOPs (absolute and absolute,X)
                program = {opcode, 0x34, 0x12};
                expected_pc = initial_pc + 3;
            }
            
            // Store initial register state
            uint8_t initial_a = cpu.get_registers()[CpuReg::A];
            uint8_t initial_x = cpu.get_registers()[CpuReg::X];
            uint8_t initial_y = cpu.get_registers()[CpuReg::Y];
            uint8_t initial_p = cpu.get_registers()[CpuReg::P];
            uint8_t initial_s = cpu.get_registers()[CpuReg::S];
            
            // Execute the NOP instruction
            run_test_case(cpu, program);
            
            // Verify the instruction executed correctly
            bool pc_correct = (cpu.get_registers()[CpuReg::PC] == expected_pc);
            bool registers_unchanged = (
                cpu.get_registers()[CpuReg::A] == initial_a &&
                cpu.get_registers()[CpuReg::X] == initial_x &&
                cpu.get_registers()[CpuReg::Y] == initial_y &&
                cpu.get_registers()[CpuReg::P] == initial_p &&
                cpu.get_registers()[CpuReg::S] == initial_s
            );
            
            if (pc_correct && registers_unchanged) {
                std::cout << "✅ NOP 0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)opcode << " - PASS" << std::endl;
                passed++;
            } else {
                std::cout << "❌ NOP 0x" << std::hex << std::setw(2) << std::setfill('0') 
                          << (int)opcode << " - FAIL";
                if (!pc_correct) {
                    std::cout << " (PC: expected 0x" << std::hex << expected_pc 
                              << ", got 0x" << cpu.get_registers()[CpuReg::PC] << ")";
                }
                if (!registers_unchanged) {
                    std::cout << " (registers changed)";
                }
                std::cout << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ NOP 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)opcode << " - EXCEPTION: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "❌ NOP 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)opcode << " - UNKNOWN EXCEPTION" << std::endl;
        }
    }
    
    std::cout << "\n=== Priority 7 Results ===" << std::endl;
    std::cout << "Passed: " << std::dec << passed << "/" << total << std::endl;
    std::cout << "Success Rate: " << (100.0 * passed / total) << "%" << std::endl;
    
    if (passed == total) {
        std::cout << "🎉 PRIORITY 7 COMPLETED! All NOP variants working!" << std::endl;
        return 0;
    } else {
        std::cout << "⚠️  Some NOP variants need fixing..." << std::endl;
        return 1;
    }
}