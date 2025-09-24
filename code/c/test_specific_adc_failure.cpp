#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/unified_helpers.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== Testing ADC Specific Failure Case ===" << std::endl;
    std::cout << "Case: ADC #$D5 with specific initial state" << std::endl;
    
    // Simulate the failing test case manually
    type_safe_array<uint8_t, 14> reg;
    
    // From the ProcessorTests failure, we need to figure out initial state
    // Let's test with a simple case first to verify our logic
    
    std::cout << "\n--- Manual ADC Test ---" << std::endl;
    
    // Test case: let's assume A=$40, immediate=$D5, C=0 initially
    // This should cause overflow (positive + negative with sign flip)
    reg[CpuReg::A] = 0x40;     // Positive number
    reg[CpuReg::P] = 0x2C;     // D=1, I=1, but C=0 initially
    uint8_t data = 0xD5;       // Negative number in signed arithmetic
    
    std::cout << "Before ADC:" << std::endl;
    std::cout << "A = $" << std::hex << std::setfill('0') << std::setw(2) << (int)reg[CpuReg::A] << std::endl;
    std::cout << "P = $" << std::setw(2) << (int)reg[CpuReg::P] << std::endl;
    std::cout << "Data = $" << std::setw(2) << (int)data << std::endl;
    
    // Manual calculation
    uint8_t a = reg[CpuReg::A];
    uint8_t carry_in = (reg[CpuReg::P] & P_CARRY) ? 1 : 0;
    uint16_t temp = a + data + carry_in;
    uint8_t result = temp & 0xFF;
    
    // Manual overflow check
    bool manual_overflow = ((~(a ^ data) & (a ^ result) & 0x80) != 0);
    bool manual_carry = temp > 0xFF;
    bool manual_negative = (result & 0x80) != 0;
    bool manual_zero = result == 0;
    
    std::cout << "\nManual calculation:" << std::endl;
    std::cout << "Result = $" << std::setw(2) << (int)result << std::endl;
    std::cout << "Carry = " << manual_carry << std::endl;
    std::cout << "Overflow = " << manual_overflow << std::endl;
    std::cout << "Negative = " << manual_negative << std::endl;
    std::cout << "Zero = " << manual_zero << std::endl;
    
    // Test our unified function
    using BusConfig = config_6502;
    alu_adc_unified<BusConfig>(reg, data);
    
    std::cout << "\nAfter unified ADC:" << std::endl;
    std::cout << "A = $" << std::setw(2) << (int)reg[CpuReg::A] << std::endl;
    std::cout << "P = $" << std::setw(2) << (int)reg[CpuReg::P] << std::endl;
    
    // Extract flags
    bool func_carry = (reg[CpuReg::P] & P_CARRY) != 0;
    bool func_overflow = (reg[CpuReg::P] & P_OVERFLOW) != 0;
    bool func_negative = (reg[CpuReg::P] & P_NEGATIVE) != 0;
    bool func_zero = (reg[CpuReg::P] & P_ZERO) != 0;
    
    std::cout << "Function results:" << std::endl;
    std::cout << "Carry = " << func_carry << std::endl;
    std::cout << "Overflow = " << func_overflow << std::endl;
    std::cout << "Negative = " << func_negative << std::endl;
    std::cout << "Zero = " << func_zero << std::endl;
    
    // Compare
    std::cout << "\n--- Comparison ---" << std::endl;
    std::cout << "Result match: " << (result == reg[CpuReg::A] ? "PASS" : "FAIL") << std::endl;
    std::cout << "Carry match: " << (manual_carry == func_carry ? "PASS" : "FAIL") << std::endl;
    std::cout << "Overflow match: " << (manual_overflow == func_overflow ? "PASS" : "FAIL") << std::endl;
    std::cout << "Negative match: " << (manual_negative == func_negative ? "PASS" : "FAIL") << std::endl;
    std::cout << "Zero match: " << (manual_zero == func_zero ? "PASS" : "FAIL") << std::endl;
    
    return 0;
}