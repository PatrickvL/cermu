#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

// Helper to print binary representation of a byte
void print_binary(uint8_t val, const std::string& label) {
    std::cout << label << " = 0x" << std::hex << (int)val << " (";
    for (int i = 7; i >= 0; i--) {
        std::cout << ((val >> i) & 1);
        if (i == 4) std::cout << " ";
    }
    std::cout << ")" << std::dec << std::endl;
}

int main() {
    std::cout << "=== Debugging Flag Order Issue ===\n";
    
    // Test the actual flag manipulation logic step by step
    uint8_t P_CARRY = 0x01, P_ZERO = 0x02, P_OVERFLOW = 0x40, P_NEGATIVE = 0x80;
    uint8_t P_DECIMAL = 0x08, P_IRQ_DIS = 0x04;
    
    // Simulate one of the failing test cases
    // Let's say we have: A=some_value, operand=some_value, result=some_value
    // And we expect V=1, N=0 but we're getting V=0, N=1
    
    std::cout << "Simulating ADC flag calculation:\n";
    
    // Let's assume initial P register state
    uint8_t p_register = P_DECIMAL | P_IRQ_DIS | P_CARRY; // 0x0D = 0000 1101
    print_binary(p_register, "Initial P");
    
    // Step 1: Calculate overflow and carry flags 
    uint8_t overflow_flag = P_OVERFLOW; // Let's say overflow should be set
    uint8_t carry_flag = P_CARRY;       // Let's say carry should be set
    uint8_t flags = carry_flag | overflow_flag; // 0x41
    
    print_binary(flags, "Computed flags (C|V)");
    
    // Step 2: Apply C and V flags (line 109 equivalent)
    p_register = (p_register & ~(P_CARRY | P_OVERFLOW)) | flags;
    print_binary(p_register, "After C|V setting");
    
    // Step 3: Calculate result value (let's say result should have N=0)
    uint8_t result = 0x50; // Positive number, so N should be 0
    print_binary(result, "Result value");
    
    // Step 4: Apply N/Z flags (line 111 equivalent - set_nz_flags)
    uint8_t NZ_MASK = P_NEGATIVE | P_ZERO;
    uint8_t nz_flags = ((result == 0) ? P_ZERO : 0) | (result & P_NEGATIVE);
    
    print_binary(NZ_MASK, "NZ_MASK");
    print_binary(nz_flags, "Computed NZ flags");
    
    p_register = (p_register & ~NZ_MASK) | nz_flags;
    print_binary(p_register, "Final P register");
    
    std::cout << "\nExpected: V=1, N=0\n";
    std::cout << "Actual:   V=" << ((p_register & P_OVERFLOW) ? 1 : 0) 
              << ", N=" << ((p_register & P_NEGATIVE) ? 1 : 0) << std::endl;
    
    return 0;
}