#include "src/chip/cpu/fam65xx_cpp/cpu.hpp"
#include <iostream>

int main() {
    using namespace fam65xx_cpp;
    
    // Test the failing case: SBC immediate with carry flag issue
    // Expected: C=0, Got: C=1
    
    // Create CPU instance
    MOS6502<bus_config_6502> cpu;
    cpu.reset();
    
    // Set up test case from processor tests
    cpu.reg[CpuReg::A] = 0x08;      // A register
    cpu.reg[CpuReg::P] = 0x24;      // Flags (carry clear = borrow will occur)
    
    std::cout << "=== SBC Carry Flag Debug ===" << std::endl;
    std::cout << "Before SBC:" << std::endl;
    std::cout << "  A = 0x" << std::hex << (int)cpu.reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << (int)cpu.reg[CpuReg::P] << std::endl;
    std::cout << "  Carry = " << ((cpu.reg[CpuReg::P] & P_CARRY) ? 1 : 0) << std::endl;
    
    // Perform SBC with data = 0xC4
    uint8_t data = 0xC4;
    uint8_t a = cpu.reg[CpuReg::A];
    uint8_t borrow = (cpu.reg[CpuReg::P] & P_CARRY) ? 0 : 1;
    
    std::cout << "\nSBC calculation:" << std::endl;
    std::cout << "  data = 0x" << std::hex << (int)data << std::endl;
    std::cout << "  borrow = " << (int)borrow << " (since carry=0)" << std::endl;
    
    // Show the problematic calculation
    uint16_t temp_unsigned = a - data - borrow;  // This wraps!
    int16_t temp_signed = (int16_t)a - (int16_t)data - (int16_t)borrow;  // Correct
    
    std::cout << "\nProblematic unsigned calculation:" << std::endl;
    std::cout << "  temp (uint16_t) = 0x" << std::hex << temp_unsigned << std::endl;
    std::cout << "  temp >= 0? " << (temp_unsigned >= 0 ? "true" : "false") << " (always true!)" << std::endl;
    
    std::cout << "\nCorrect signed calculation:" << std::endl;
    std::cout << "  temp (int16_t) = " << std::dec << temp_signed << std::endl;
    std::cout << "  temp >= 0? " << (temp_signed >= 0 ? "true" : "false") << std::endl;
    
    // Test the unified helper directly
    alu_sbc_unified<bus_config_6502>(cpu.reg, data);
    
    std::cout << "\nAfter SBC (current implementation):" << std::endl;
    std::cout << "  A = 0x" << std::hex << (int)cpu.reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << (int)cpu.reg[CpuReg::P] << std::endl;
    std::cout << "  Carry = " << ((cpu.reg[CpuReg::P] & P_CARRY) ? 1 : 0) << std::endl;
    
    std::cout << "\nExpected result:" << std::endl;
    std::cout << "  A = 0x" << std::hex << (temp_signed & 0xFF) << std::endl;
    std::cout << "  Carry = " << (temp_signed >= 0 ? 1 : 0) << " (should be 0)" << std::endl;
    
    return 0;
}