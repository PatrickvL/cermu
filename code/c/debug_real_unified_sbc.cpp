#include "src/chip/cpu/fam65xx_cpp/unified_helpers.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include <iostream>
#include <array>

// Simple bus config for testing
struct TestBusConfig {
    static constexpr bool has_decimal_mode = true;
    static constexpr bool has_cmos_fixes = false;
};

int main() {
    using namespace fam65xx_cpp;
    
    // Create register array matching our CPU implementation
    std::array<uint8_t, 16> reg = {0};
    
    // Set up the exact test case using integer indices
    reg[static_cast<int>(CpuReg::A)] = 156;   // Initial A = 156 (0x9C)
    reg[static_cast<int>(CpuReg::P)] = 109;   // Initial P = 109 (0x6D)
    uint8_t data = 196;     // Data = 196 (0xC4)
    
    std::cout << "=== Real Unified SBC Test ===" << std::endl;
    std::cout << "Before: A=0x" << std::hex << (int)reg[static_cast<int>(CpuReg::A)] << ", P=0x" << (int)reg[static_cast<int>(CpuReg::P)] << std::endl;
    std::cout << "Data: 0x" << (int)data << std::endl;
    std::cout << "Initial carry: " << (reg[static_cast<int>(CpuReg::P)] & P_CARRY ? 1 : 0) << std::endl;
    
    // Call the actual unified helper function
    alu_sbc_unified<TestBusConfig>(reg, data);
    
    std::cout << "After: A=0x" << (int)reg[static_cast<int>(CpuReg::A)] << ", P=0x" << (int)reg[static_cast<int>(CpuReg::P)] << std::endl;
    std::cout << "Final carry: " << (reg[static_cast<int>(CpuReg::P)] & P_CARRY ? 1 : 0) << std::endl;
    
    std::cout << "\nExpected: A=0x78, P=0xAC" << std::endl;
    std::cout << "Expected carry: 0" << std::endl;
    
    if (reg[static_cast<int>(CpuReg::A)] == 0x78) {
        std::cout << "✓ A register MATCHES expected!" << std::endl;
    } else {
        std::cout << "✗ A register MISMATCH!" << std::endl;
    }
    
    if (reg[static_cast<int>(CpuReg::P)] == 0xAC) {
        std::cout << "✓ P register MATCHES expected!" << std::endl;
    } else {
        std::cout << "✗ P register MISMATCH!" << std::endl;
    }
    
    return 0;
}