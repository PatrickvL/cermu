#include "tests/fam65xx_cpp_test_harness.h"
#include <iostream>

int main() {
    std::cout << "=== RTI CYCLE FIX VALIDATION ===" << std::endl;
    
    auto cpu = create_test_cpu();
    
    // Set up RTI test case
    cpu.reg[CpuReg::PC] = 0x8000;
    cpu.reg[CpuReg::SP] = 0xFC;  // SP will be incremented during RTI
    
    // Set up memory
    memory[0x8000] = 0x40;       // RTI opcode
    memory[0x01FD] = 0x30;       // Status register on stack
    memory[0x01FE] = 0x34;       // PC low on stack
    memory[0x01FF] = 0x12;       // PC high on stack
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.reg[CpuReg::PC] << std::endl;
    std::cout << "SP: 0x" << std::hex << (int)cpu.reg[CpuReg::SP] << std::endl;
    
    int cycle_count = 0;
    bool instruction_complete = false;
    
    // Execute RTI instruction cycle by cycle
    while (!instruction_complete && cycle_count < 10) {
        uint8_t data = 0;
        uint16_t addr = 0;
        bool rw = true;
        
        // Execute one cycle
        bus_state_t bus_state = create_bus_state(data);
        auto result = cpu.execute_cycle(bus_state);
        
        cycle_count++;
        instruction_complete = result.instruction_complete;
        
        std::cout << "Cycle " << cycle_count << ": ";
        std::cout << "Addr=0x" << std::hex << result.address << ", ";
        std::cout << "Data=0x" << std::hex << (int)result.data << ", ";
        std::cout << "RW=" << (result.rw ? "R" : "W") << ", ";
        std::cout << "Complete=" << (instruction_complete ? "YES" : "NO") << std::endl;
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.reg[CpuReg::PC] << std::endl;
    std::cout << "SP: 0x" << std::hex << (int)cpu.reg[CpuReg::SP] << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.reg[CpuReg::P] << std::endl;
    std::cout << "Total cycles: " << cycle_count << std::endl;
    
    // Verify cycle count
    if (cycle_count == 6) {
        std::cout << "\n✅ SUCCESS: RTI executed in exactly 6 cycles!" << std::endl;
    } else {
        std::cout << "\n❌ FAILURE: RTI executed in " << cycle_count << " cycles (expected 6)" << std::endl;
    }
    
    // Verify final PC
    uint16_t expected_pc = 0x1234;
    if (cpu.reg[CpuReg::PC] == expected_pc) {
        std::cout << "✅ SUCCESS: PC correctly restored to 0x" << std::hex << expected_pc << std::endl;
    } else {
        std::cout << "❌ FAILURE: PC is 0x" << std::hex << cpu.reg[CpuReg::PC] << " (expected 0x" << expected_pc << ")" << std::endl;
    }
    
    // Verify final status register
    uint8_t expected_p = 0x30;
    if (cpu.reg[CpuReg::P] == expected_p) {
        std::cout << "✅ SUCCESS: P correctly restored to 0x" << std::hex << (int)expected_p << std::endl;
    } else {
        std::cout << "❌ FAILURE: P is 0x" << std::hex << (int)cpu.reg[CpuReg::P] << " (expected 0x" << (int)expected_p << ")" << std::endl;
    }
    
    return 0;
}