#include "tests/fam65xx_cpp_test_harness.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== JMP ABSOLUTE REGRESSION DEBUG ===" << std::endl;
    
    // Test with NMOS 6502 configuration
    using TestConfig = cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, 16, false, false, false>;
    fam65xx<TestConfig> cpu;
    TestHarness<TestConfig> harness(cpu);
    
    // Test case 1: Simple JMP absolute
    std::cout << "\n=== TEST 1: Simple JMP absolute ===" << std::endl;
    harness.reset_cpu_state();
    harness.set_register(CpuReg::PC, 0x8000);
    
    // Set up JMP $1234 instruction
    harness.write_memory(0x8000, 0x4C);  // JMP absolute opcode
    harness.write_memory(0x8001, 0x34);  // Low byte of target address
    harness.write_memory(0x8002, 0x12);  // High byte of target address
    
    std::cout << "Initial PC: 0x" << std::hex << harness.get_register(CpuReg::PC) << std::endl;
    
    // Execute instruction
    int cycle_count = 0;
    while (!cpu.is_instruction_complete() && cycle_count < 10) {
        cpu.execute_cycle();
        cycle_count++;
        std::cout << "Cycle " << cycle_count << ": PC=0x" << std::hex 
                  << harness.get_register(CpuReg::PC) << std::endl;
    }
    
    std::cout << "Final PC: 0x" << std::hex << harness.get_register(CpuReg::PC) << std::endl;
    std::cout << "Expected PC: 0x1234" << std::endl;
    std::cout << "Cycles: " << cycle_count << std::endl;
    
    if (harness.get_register(CpuReg::PC) == 0x1234) {
        std::cout << "✅ TEST 1 PASSED" << std::endl;
    } else {
        std::cout << "❌ TEST 1 FAILED" << std::endl;
    }
    
    // Test case 2: Edge case with page boundary
    std::cout << "\n=== TEST 2: Page boundary JMP absolute ===" << std::endl;
    harness.reset_cpu_state();
    harness.set_register(CpuReg::PC, 0x80FF);
    
    // Set up JMP $ABCD instruction at page boundary
    harness.write_memory(0x80FF, 0x4C);  // JMP absolute opcode
    harness.write_memory(0x8100, 0xCD);  // Low byte of target address (crosses page)
    harness.write_memory(0x8101, 0xAB);  // High byte of target address
    
    std::cout << "Initial PC: 0x" << std::hex << harness.get_register(CpuReg::PC) << std::endl;
    
    // Execute instruction
    cycle_count = 0;
    while (!cpu.is_instruction_complete() && cycle_count < 10) {
        cpu.execute_cycle();
        cycle_count++;
        std::cout << "Cycle " << cycle_count << ": PC=0x" << std::hex 
                  << harness.get_register(CpuReg::PC) << std::endl;
    }
    
    std::cout << "Final PC: 0x" << std::hex << harness.get_register(CpuReg::PC) << std::endl;
    std::cout << "Expected PC: 0xABCD" << std::endl;
    std::cout << "Cycles: " << cycle_count << std::endl;
    
    if (harness.get_register(CpuReg::PC) == 0xABCD) {
        std::cout << "✅ TEST 2 PASSED" << std::endl;
    } else {
        std::cout << "❌ TEST 2 FAILED" << std::endl;
    }
    
    // Test case 3: Check flag preservation 
    std::cout << "\n=== TEST 3: Flag preservation ===" << std::endl;
    harness.reset_cpu_state();
    harness.set_register(CpuReg::PC, 0x9000);
    harness.set_register(CpuReg::P, 0xFF);  // Set all flags
    
    // Set up JMP $5678 instruction
    harness.write_memory(0x9000, 0x4C);  // JMP absolute opcode
    harness.write_memory(0x9001, 0x78);  // Low byte of target address
    harness.write_memory(0x9002, 0x56);  // High byte of target address
    
    std::cout << "Initial P: 0x" << std::hex << harness.get_register(CpuReg::P) << std::endl;
    
    // Execute instruction
    cycle_count = 0;
    while (!cpu.is_instruction_complete() && cycle_count < 10) {
        cpu.execute_cycle();
        cycle_count++;
    }
    
    std::cout << "Final PC: 0x" << std::hex << harness.get_register(CpuReg::PC) << std::endl;
    std::cout << "Final P: 0x" << std::hex << harness.get_register(CpuReg::P) << std::endl;
    std::cout << "Expected PC: 0x5678" << std::endl;
    std::cout << "Expected P: 0xFF (unchanged)" << std::endl;
    
    if (harness.get_register(CpuReg::PC) == 0x5678 && harness.get_register(CpuReg::P) == 0xFF) {
        std::cout << "✅ TEST 3 PASSED" << std::endl;
    } else {
        std::cout << "❌ TEST 3 FAILED" << std::endl;
    }
    
    return 0;
}