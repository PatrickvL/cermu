#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

void test_adc_specific() {
    std::cout << "=== ADC Debugging Test - Exact ProcessorTests Case ===" << std::endl;
    
    // Create CPU instance using correct config name
    fam65xx_cpp::fam65xx<config_6502> cpu;
    
    // Initialize for testing (avoiding reset state)
    cpu.init_for_test();
    
    // EXACT TEST CASE FROM ProcessorTests: "69 1b 91"
    // Initial: a: 76, p: 228  Final: a: 103, p: 36
    // 76 = 0x4c, 228 = 0xe4, 103 = 0x67, 36 = 0x24
    
    // Set up EXACT initial state from ProcessorTests
    cpu.set_a(76);       // A register = 0x4c
    cpu.set_p(228);      // Status register = 0xe4
    
    std::cout << "EXACT ProcessorTests initial state:" << std::endl;
    std::cout << "A = " << (int)cpu.get_a() << " (0x" << std::hex << std::setw(2) << std::setfill('0')
              << (int)cpu.get_a() << ")" << std::dec << std::endl;
    std::cout << "P = " << (int)cpu.get_p() << " (0x" << std::hex << std::setw(2) << std::setfill('0')
              << (int)cpu.get_p() << ")" << std::dec << std::endl;
    std::cout << "Carry flag = " << ((cpu.get_p() & 0x01) ? 1 : 0) << std::endl;
    
    // ADC immediate with 0x1b (27)
    uint8_t data = 27;  // 0x1b
    
    // Manual calculation
    uint8_t a = cpu.get_a();
    uint8_t carry_in = (cpu.get_p() & 0x01) ? 1 : 0;
    uint16_t temp = a + data + carry_in;
    uint8_t result = temp & 0xFF;
    
    std::cout << "Manual calculation:" << std::endl;
    std::cout << (int)a << " + " << (int)data << " + " << (int)carry_in
              << " = " << temp << " (0x" << std::hex << temp << ")" << std::dec << std::endl;
    std::cout << "Result = " << (int)result << " (0x" << std::hex << (int)result << ")" << std::dec << std::endl;
    
    // Create a register array and set values for ALU test
    CpuRegisterArray test_reg;
    test_reg[CpuReg::A] = cpu.get_a();
    test_reg[CpuReg::P] = cpu.get_p();
    
    // Execute using ALU
    fam65xx_cpp::AluOperations<config_6502>::execute_alu_operation(
        test_reg, AluOp::ADC, data);
    
    std::cout << "After ADC:" << std::endl;
    std::cout << "A = " << (int)test_reg[CpuReg::A] << " (0x" << std::hex << std::setw(2) << std::setfill('0')
              << (int)test_reg[CpuReg::A] << ")" << std::dec << std::endl;
    std::cout << "P = " << (int)test_reg[CpuReg::P] << " (0x" << std::hex << std::setw(2) << std::setfill('0')
              << (int)test_reg[CpuReg::P] << ")" << std::dec << std::endl;
    
    std::cout << "\nProcessorTests expectations:" << std::endl;
    std::cout << "Expected A = 103 (0x67)" << std::endl;
    std::cout << "Expected P = 36 (0x24)" << std::endl;
    std::cout << "Actual A   = " << (int)test_reg[CpuReg::A] << " (0x" << std::hex << (int)test_reg[CpuReg::A] << ")" << std::dec << std::endl;
    std::cout << "Actual P   = " << (int)test_reg[CpuReg::P] << " (0x" << std::hex << (int)test_reg[CpuReg::P] << ")" << std::dec << std::endl;
    std::cout << "A Match: " << ((test_reg[CpuReg::A] == 103) ? "YES" : "NO") << std::endl;
    std::cout << "P Match: " << ((test_reg[CpuReg::P] == 36) ? "YES" : "NO") << std::endl;
}

int main() {
    test_adc_specific();
    return 0;
}