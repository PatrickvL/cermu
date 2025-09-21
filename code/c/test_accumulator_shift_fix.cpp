// Test to verify accumulator shift operations are working correctly
#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/alu_operations.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== Testing Accumulator Shift Operations Fix ===" << std::endl;
    
    // Create CPU with 6502 configuration
    auto cpu = fam65xx<config_6502>();
    
    // Test ASL A (0x0A) - Arithmetic Shift Left Accumulator
    std::cout << "\n--- Testing ASL A (0x0A) ---" << std::endl;
    
    // Reset CPU
    cpu.init_for_test();
    
    // Set accumulator to 0x42 (binary: 01000010)
    cpu.set_a(0x42);
    cpu.set_p(0x00); // Clear all flags
    
    std::cout << "Before ASL A:" << std::endl;
    std::cout << "  A = 0x" << std::hex << std::uppercase << (int)cpu.get_a() << std::endl;
    std::cout << "  P = 0x" << std::hex << std::uppercase << (int)cpu.get_p() << std::endl;
    
    // Test the new accumulator-specific ALU operation directly
    // Create a test register array
    CpuRegisterArray test_reg;
    test_reg[CpuReg::A] = 0x42;
    test_reg[CpuReg::P] = 0x00;
    
    // Execute ASL_ACC ALU operation directly
    AluOperations<config_6502>::execute_alu_operation(
        test_reg, 
        AluOp::ASL_ACC, 
        0x42  // Input data (should be ignored for accumulator operations)
    );
    
    std::cout << "After ASL_ACC:" << std::endl;
    std::cout << "  A = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::P] << std::endl;
    
    // Expected: A = 0x84, P = 0x80 (N flag set)
    bool test1_passed = (test_reg[CpuReg::A] == 0x84) && 
                        (test_reg[CpuReg::P] & P_NEGATIVE);
    
    std::cout << "Test Result: " << (test1_passed ? "PASS" : "FAIL") << std::endl;
    if (test1_passed) {
        std::cout << "✅ ASL_ACC correctly shifts accumulator: 0x42 -> 0x84, sets N flag" << std::endl;
    } else {
        std::cout << "❌ ASL_ACC failed - Expected A=0x84, P=0x80+" << std::endl;
    }
    
    // Test LSR A (0x4A) - Logical Shift Right Accumulator
    std::cout << "\n--- Testing LSR A (0x4A) ---" << std::endl;
    
    // Set accumulator to 0x85 (binary: 10000101) 
    test_reg[CpuReg::A] = 0x85;
    test_reg[CpuReg::P] = 0x00; // Clear all flags
    
    std::cout << "Before LSR A:" << std::endl;
    std::cout << "  A = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::P] << std::endl;
    
    // Execute LSR_ACC ALU operation
    AluOperations<config_6502>::execute_alu_operation(
        test_reg, 
        AluOp::LSR_ACC, 
        0x85  // Input data (should be ignored for accumulator operations)
    );
    
    std::cout << "After LSR_ACC:" << std::endl;
    std::cout << "  A = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::P] << std::endl;
    
    // Expected: A = 0x42, P = 0x01 (C flag set from bit 0)
    bool test2_passed = (test_reg[CpuReg::A] == 0x42) && 
                        (test_reg[CpuReg::P] & P_CARRY);
    
    std::cout << "Test Result: " << (test2_passed ? "PASS" : "FAIL") << std::endl;
    if (test2_passed) {
        std::cout << "✅ LSR_ACC correctly shifts accumulator: 0x85 -> 0x42, sets C flag" << std::endl;
    } else {
        std::cout << "❌ LSR_ACC failed - Expected A=0x42, P=0x01+" << std::endl;
    }
    
    // Test ROL A (0x2A) - Rotate Left Accumulator
    std::cout << "\n--- Testing ROL A (0x2A) ---" << std::endl;
    
    // Set accumulator to 0x80, carry flag set
    test_reg[CpuReg::A] = 0x80;
    test_reg[CpuReg::P] = P_CARRY; // Set carry flag
    
    std::cout << "Before ROL A:" << std::endl;
    std::cout << "  A = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::P] << std::endl;
    
    // Execute ROL_ACC ALU operation
    AluOperations<config_6502>::execute_alu_operation(
        test_reg, 
        AluOp::ROL_ACC, 
        0x80  // Input data (should be ignored for accumulator operations)
    );
    
    std::cout << "After ROL_ACC:" << std::endl;
    std::cout << "  A = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::P] << std::endl;
    
    // Expected: A = 0x01 (0x80 << 1 + carry), P = 0x01 (C flag set from bit 7)
    bool test3_passed = (test_reg[CpuReg::A] == 0x01) && 
                        (test_reg[CpuReg::P] & P_CARRY);
    
    std::cout << "Test Result: " << (test3_passed ? "PASS" : "FAIL") << std::endl;
    if (test3_passed) {
        std::cout << "✅ ROL_ACC correctly rotates accumulator: 0x80 -> 0x01, sets C flag" << std::endl;
    } else {
        std::cout << "❌ ROL_ACC failed - Expected A=0x01, P=0x01+" << std::endl;
    }
    
    // Test ROR A (0x6A) - Rotate Right Accumulator
    std::cout << "\n--- Testing ROR A (0x6A) ---" << std::endl;
    
    // Set accumulator to 0x01, carry flag clear
    test_reg[CpuReg::A] = 0x01;
    test_reg[CpuReg::P] = 0x00; // Clear all flags
    
    std::cout << "Before ROR A:" << std::endl;
    std::cout << "  A = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::P] << std::endl;
    
    // Execute ROR_ACC ALU operation
    AluOperations<config_6502>::execute_alu_operation(
        test_reg, 
        AluOp::ROR_ACC, 
        0x01  // Input data (should be ignored for accumulator operations)
    );
    
    std::cout << "After ROR_ACC:" << std::endl;
    std::cout << "  A = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::A] << std::endl;
    std::cout << "  P = 0x" << std::hex << std::uppercase << (int)test_reg[CpuReg::P] << std::endl;
    
    // Expected: A = 0x00 (0x01 >> 1), P = 0x03 (C flag set from bit 0, Z flag set)
    bool test4_passed = (test_reg[CpuReg::A] == 0x00) && 
                        (test_reg[CpuReg::P] & P_CARRY) &&
                        (test_reg[CpuReg::P] & P_ZERO);
    
    std::cout << "Test Result: " << (test4_passed ? "PASS" : "FAIL") << std::endl;
    if (test4_passed) {
        std::cout << "✅ ROR_ACC correctly rotates accumulator: 0x01 -> 0x00, sets C and Z flags" << std::endl;
    } else {
        std::cout << "❌ ROR_ACC failed - Expected A=0x00, P=0x03+" << std::endl;
    }
    
    bool all_tests_passed = test1_passed && test2_passed && test3_passed && test4_passed;
    
    std::cout << "\n=== Overall Result: " << (all_tests_passed ? "SUCCESS" : "FAILURE") << " ===" << std::endl;
    
    if (all_tests_passed) {
        std::cout << "✅ All accumulator shift/rotate operations are working correctly!" << std::endl;
        std::cout << "✅ ASL_ACC, LSR_ACC, ROL_ACC, ROR_ACC directly modify the accumulator register" << std::endl;
        std::cout << "✅ The fix for accumulator vs memory mode is working" << std::endl;
        std::cout << "✅ New accumulator-specific ALU operations are operational" << std::endl;
    } else {
        std::cout << "❌ There are still issues with accumulator operations" << std::endl;
    }
    
    return all_tests_passed ? 0 : 1;
}