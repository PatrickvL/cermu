#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

int main() {
    // Create a 6502 CPU
    MOS6502<DefaultBusConfig> cpu;
    
    std::cout << "=== Direct ALU Operation Test ===" << std::endl;
    
    // Test ASL_ACC operation directly
    std::cout << "\nTesting ASL_ACC operation:" << std::endl;
    
    // Setup initial state
    cpu.reset();
    cpu.reg[CpuReg::A] = 0x42;  // Binary: 01000010
    cpu.reg[CpuReg::P] = 0x00;  // Clear all flags
    
    std::cout << "Before ASL_ACC: A=0x" << std::hex << (int)cpu.reg[CpuReg::A] 
              << " P=0x" << std::hex << (int)cpu.reg[CpuReg::P] << std::endl;
    
    // Execute ASL_ACC operation directly
    cpu.execute_alu_operation(AluOp::ASL_ACC, 0x00);  // Data doesn't matter for accumulator ops
    
    std::cout << "After ASL_ACC:  A=0x" << std::hex << (int)cpu.reg[CpuReg::A] 
              << " P=0x" << std::hex << (int)cpu.reg[CpuReg::P] << std::endl;
    
    // Expected: A = 0x84 (binary: 10000100), P = 0x80 (N flag set)
    std::cout << "Expected:       A=0x84 P=0x80" << std::endl;
    
    // Test LSR_ACC operation
    std::cout << "\nTesting LSR_ACC operation:" << std::endl;
    
    cpu.reset();
    cpu.reg[CpuReg::A] = 0x85;  // Binary: 10000101
    cpu.reg[CpuReg::P] = 0x00;  // Clear all flags
    
    std::cout << "Before LSR_ACC: A=0x" << std::hex << (int)cpu.reg[CpuReg::A] 
              << " P=0x" << std::hex << (int)cpu.reg[CpuReg::P] << std::endl;
    
    cpu.execute_alu_operation(AluOp::LSR_ACC, 0x00);
    
    std::cout << "After LSR_ACC:  A=0x" << std::hex << (int)cpu.reg[CpuReg::A] 
              << " P=0x" << std::hex << (int)cpu.reg[CpuReg::P] << std::endl;
    
    // Expected: A = 0x42 (binary: 01000010), P = 0x01 (C flag set)
    std::cout << "Expected:       A=0x42 P=0x01" << std::endl;
    
    return 0;
}