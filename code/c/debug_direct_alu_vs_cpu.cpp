// Compare direct ALU operation vs full CPU execution for ASL A
#include <iostream>
#include <iomanip>
#include <vector>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/alu_operations.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== Direct ALU vs Full CPU Execution Comparison ===" << std::endl;
    
    // Test 1: Direct ALU operation
    std::cout << "\n--- Test 1: Direct ALU Operation ---" << std::endl;
    CpuRegisterArray test_reg;
    test_reg[CpuReg::A] = 0x42;
    test_reg[CpuReg::P] = 0x00;
    
    std::cout << "Before ALU: A=0x" << std::hex << (int)test_reg[CpuReg::A] 
              << " P=0x" << std::hex << (int)test_reg[CpuReg::P] << std::endl;
    
    AluOperations<config_6502>::execute_alu_operation(test_reg, AluOp::ASL_ACC, 0x42);
    
    std::cout << "After ALU:  A=0x" << std::hex << (int)test_reg[CpuReg::A] 
              << " P=0x" << std::hex << (int)test_reg[CpuReg::P] << std::endl;
    
    // Test 2: Full CPU execution
    std::cout << "\n--- Test 2: Full CPU Execution ---" << std::endl;
    auto cpu = fam65xx<config_6502>();
    cpu.init_for_test();
    
    cpu.set_pc(0x0000);
    cpu.set_a(0x42);
    cpu.set_p(0x00);
    
    std::cout << "Before CPU: A=0x" << std::hex << (int)cpu.get_a() 
              << " P=0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Memory with ASL A instruction
    std::vector<uint8_t> memory(65536, 0xEA); // Fill with NOP
    memory[0x0000] = 0x0A; // ASL A instruction
    
    // Execute just the opcode fetch and ALU cycle
    bus_state_t bus_state = 0;
    
    // Cycle 0: Opcode fetch
    uint16_t addr = cpu.get_address();
    uint8_t data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, data);
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 0: A=0x" << std::hex << (int)cpu.get_a() 
              << " P=0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Cycle 1: Execute ASL A
    addr = cpu.get_address();
    data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, data);
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    cpu.cycle_tick(bus_state);
    
    std::cout << "After CPU:  A=0x" << std::hex << (int)cpu.get_a() 
              << " P=0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Compare results
    std::cout << "\n--- Comparison ---" << std::endl;
    bool a_match = (test_reg[CpuReg::A] == cpu.get_a());
    bool p_match = (test_reg[CpuReg::P] == cpu.get_p());
    
    std::cout << "A register match: " << (a_match ? "YES" : "NO") << std::endl;
    std::cout << "P register match: " << (p_match ? "YES" : "NO") << std::endl;
    
    if (!p_match) {
        uint8_t alu_p = test_reg[CpuReg::P];
        uint8_t cpu_p = cpu.get_p();
        std::cout << "Flag difference analysis:" << std::endl;
        std::cout << "  ALU P: 0x" << std::hex << (int)alu_p << " (binary: ";
        for (int i = 7; i >= 0; i--) {
            std::cout << ((alu_p >> i) & 1);
        }
        std::cout << ")" << std::endl;
        
        std::cout << "  CPU P: 0x" << std::hex << (int)cpu_p << " (binary: ";
        for (int i = 7; i >= 0; i--) {
            std::cout << ((cpu_p >> i) & 1);
        }
        std::cout << ")" << std::endl;
        
        uint8_t diff = alu_p ^ cpu_p;
        std::cout << "  Diff:  0x" << std::hex << (int)diff << " (binary: ";
        for (int i = 7; i >= 0; i--) {
            std::cout << ((diff >> i) & 1);
        }
        std::cout << ")" << std::endl;
        
        if (diff & 0x40) {
            std::cout << "  *** V flag (bit 6) differs! ***" << std::endl;
        }
        if (diff & 0x20) {
            std::cout << "  *** Reserved bit 5 differs! ***" << std::endl;
        }
    }
    
    return 0;
}