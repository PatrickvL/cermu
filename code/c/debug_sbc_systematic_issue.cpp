#include <iostream>
#include <iomanip>
#include <random>

// Include CPU implementation
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include "src/chip/cpu/fam65xx_cpp/fam65xx_cpp.hpp"

using namespace fam65xx_cpp;

// Test a specific SBC case to understand the systematic issue
void test_sbc_specific_case() {
    std::cout << "=== SBC Systematic Issue Analysis ===" << std::endl;
    
    // Create CPU instance
    Fam65xxCpp<CpuConfig6502> cpu;
    cpu.reset();
    
    // Simple case: A=0x50, SBC #$30, Carry=1 (no borrow)
    // Expected: A=0x20, C=1, N=0, Z=0, V=0
    auto test_state = runner.create_test_state();
    test_state.reg[CpuReg::A] = 0x50;
    test_state.reg[CpuReg::P] = P_CARRY;  // Set carry (no borrow)
    test_state.memory[0x1000] = 0xE9;     // SBC immediate
    test_state.memory[0x1001] = 0x30;     // Operand
    test_state.reg[CpuReg::PCL] = 0x00;
    test_state.reg[CpuReg::PCH] = 0x10;
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  A=$" << std::hex << (int)test_state.reg[CpuReg::A] << std::endl;
    std::cout << "  P=$" << std::hex << (int)test_state.reg[CpuReg::P] << " (C=" << ((test_state.reg[CpuReg::P] & P_CARRY) ? 1 : 0) << ")" << std::endl;
    std::cout << "  Operand=$30" << std::endl;
    
    // Execute SBC
    runner.run_single_instruction(test_state);
    
    std::cout << "After SBC:" << std::endl;
    std::cout << "  A=$" << std::hex << (int)test_state.reg[CpuReg::A] << std::endl;
    std::cout << "  P=$" << std::hex << (int)test_state.reg[CpuReg::P];
    std::cout << " (N=" << ((test_state.reg[CpuReg::P] & P_NEGATIVE) ? 1 : 0);
    std::cout << " V=" << ((test_state.reg[CpuReg::P] & P_OVERFLOW) ? 1 : 0);
    std::cout << " Z=" << ((test_state.reg[CpuReg::P] & P_ZERO) ? 1 : 0);
    std::cout << " C=" << ((test_state.reg[CpuReg::P] & P_CARRY) ? 1 : 0) << ")" << std::endl;
    
    // Manual calculation
    uint8_t a = 0x50;
    uint8_t data = 0x30;
    uint8_t borrow = 0;  // Carry is set, so no borrow
    int16_t temp = (int16_t)a - (int16_t)data - (int16_t)borrow;
    uint8_t result = temp & 0xFF;
    bool carry_flag = temp >= 0;
    bool overflow_flag = (a ^ data) & (a ^ result) & 0x80;
    
    std::cout << "Expected:" << std::endl;
    std::cout << "  A=$" << std::hex << (int)result << std::endl;
    std::cout << "  C=" << (carry_flag ? 1 : 0) << " V=" << (overflow_flag ? 1 : 0) << std::endl;
    
    if (test_state.reg[CpuReg::A] != result) {
        std::cout << "❌ Result mismatch!" << std::endl;
    }
    if (((test_state.reg[CpuReg::P] & P_CARRY) != 0) != carry_flag) {
        std::cout << "❌ Carry flag mismatch!" << std::endl;
    }
    if (((test_state.reg[CpuReg::P] & P_OVERFLOW) != 0) != overflow_flag) {
        std::cout << "❌ Overflow flag mismatch!" << std::endl;
    }
}

// Test SBC with borrow case
void test_sbc_borrow_case() {
    std::cout << "\n=== SBC Borrow Case ===" << std::endl;
    
    TestRunner runner;
    
    // Case: A=0x30, SBC #$50, Carry=0 (borrow needed)
    // Expected: A=0xDF (-33 as signed), C=0, N=1, Z=0, V=0
    auto test_state = runner.create_test_state();
    test_state.reg[CpuReg::A] = 0x30;
    test_state.reg[CpuReg::P] = 0;  // Clear carry (borrow needed)
    test_state.memory[0x1000] = 0xE9;     // SBC immediate
    test_state.memory[0x1001] = 0x50;     // Operand
    test_state.reg[CpuReg::PCL] = 0x00;
    test_state.reg[CpuReg::PCH] = 0x10;
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  A=$" << std::hex << (int)test_state.reg[CpuReg::A] << std::endl;
    std::cout << "  P=$" << std::hex << (int)test_state.reg[CpuReg::P] << " (C=" << ((test_state.reg[CpuReg::P] & P_CARRY) ? 1 : 0) << ")" << std::endl;
    
    runner.run_single_instruction(test_state);
    
    std::cout << "After SBC:" << std::endl;
    std::cout << "  A=$" << std::hex << (int)test_state.reg[CpuReg::A] << std::endl;
    std::cout << "  P=$" << std::hex << (int)test_state.reg[CpuReg::P];
    std::cout << " (N=" << ((test_state.reg[CpuReg::P] & P_NEGATIVE) ? 1 : 0);
    std::cout << " V=" << ((test_state.reg[CpuReg::P] & P_OVERFLOW) ? 1 : 0);
    std::cout << " Z=" << ((test_state.reg[CpuReg::P] & P_ZERO) ? 1 : 0);
    std::cout << " C=" << ((test_state.reg[CpuReg::P] & P_CARRY) ? 1 : 0) << ")" << std::endl;
    
    // Manual calculation
    uint8_t a = 0x30;
    uint8_t data = 0x50;
    uint8_t borrow = 1;  // Carry is clear, so borrow
    int16_t temp = (int16_t)a - (int16_t)data - (int16_t)borrow;
    uint8_t result = temp & 0xFF;
    bool carry_flag = temp >= 0;
    bool overflow_flag = (a ^ data) & (a ^ result) & 0x80;
    
    std::cout << "Expected:" << std::endl;
    std::cout << "  A=$" << std::hex << (int)result << std::endl;
    std::cout << "  C=" << (carry_flag ? 1 : 0) << " V=" << (overflow_flag ? 1 : 0) << std::endl;
    std::cout << "  temp=" << temp << std::endl;
}

int main() {
    test_sbc_specific_case();
    test_sbc_borrow_case();
    return 0;
}