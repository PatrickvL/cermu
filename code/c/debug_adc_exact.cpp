#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <tuple>

using namespace fam65xx_cpp;

int main() {
    using Config = cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, 0x10, false, false, false>;
    fam65xx<Config> cpu;
    
    std::cout << "=== ADC Test Case: 69 4a c2 ===" << std::endl;
    
    // Test case: ADC #$4a
    // Initial state needs to be reconstructed from ProcessorTests
    // Let's try a few different initial conditions
    
    struct TestCase { uint8_t a_val; uint8_t data_val; bool carry; };
    TestCase test_cases[] = {
        {0xc2, 0x4a, false}, // A=$c2, data=$4a, C=0
        {0xc2, 0x4a, true},  // A=$c2, data=$4a, C=1
        {0x4a, 0xc2, false}, // A=$4a, data=$c2, C=0
        {0x4a, 0xc2, true}   // A=$4a, data=$c2, C=1
    };
    
    for (const auto& test : test_cases) {
        uint8_t a_val = test.a_val;
        uint8_t data_val = test.data_val;
        bool carry = test.carry;
        cpu.init_for_test();
        cpu.set_a(a_val);
        cpu.set_p(carry ? (cpu.get_p() | P_CARRY) : (cpu.get_p() & ~P_CARRY));
        
        std::cout << "\n--- Test: A=$" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
                  << (int)a_val << ", data=$" << (int)data_val << ", C=" << (carry ? 1 : 0) << " ---" << std::endl;
        
        std::cout << "Before: A=$" << (int)cpu.get_a() 
                  << ", P=$" << (int)cpu.get_p() << std::endl;
        
        // Call ADC directly
        CpuRegisterArray reg;
        for (int i = 0; i < static_cast<int>(CpuReg::COUNT); ++i) {
            reg[i] = cpu.get_reg(static_cast<CpuReg>(i));
        }
        
        alu_adc_unified<Config>(reg, data_val);
        
        // Copy back
        for (int i = 0; i < static_cast<int>(CpuReg::COUNT); ++i) {
            cpu.set_reg(static_cast<CpuReg>(i), reg[i]);
        }
        
        std::cout << "After:  A=$" << (int)cpu.get_a() 
                  << ", P=$" << (int)cpu.get_p() << std::endl;
        
        // Manual calculation check
        uint16_t expected = a_val + data_val + (carry ? 1 : 0);
        uint8_t expected_a = expected & 0xFF;
        bool expected_c = expected > 0xFF;
        
        std::cout << "Manual: A=$" << (int)expected_a 
                  << ", C=" << (expected_c ? 1 : 0) << std::endl;
                  
        if (cpu.get_a() == 0xc8 && (cpu.get_p() & P_CARRY) == 0) {
            std::cout << "*** MATCHES EXPECTED ProcessorTests RESULT! ***" << std::endl;
        }
    }
    
    return 0;
}