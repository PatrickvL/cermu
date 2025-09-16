#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

int main() {
    // Create a test case for the failing "69 d5 74" test
    // This is ADC immediate with operand 0xd5
    
    fam65xx_cpp::Fam65xx<fam65xx_cpp::DefaultBusConfig> cpu;
    
    // Initialize CPU for test
    cpu.init_for_test();
    
    // Set up the failing test state
    // From the failing test, we need to load the initial state first
    // Let's manually test different register states
    
    std::cout << "=== BCD ADC Debug Test ===" << std::endl;
    
    // Test various scenarios
    for (uint8_t a_val = 0x70; a_val <= 0x80; a_val += 1) {
        for (uint8_t carry = 0; carry <= 1; carry++) {
            for (uint8_t decimal = 0; decimal <= 1; decimal++) {
                cpu.init_for_test();
                cpu.get_registers()[fam65xx_cpp::CpuReg::A] = a_val;
                cpu.get_registers()[fam65xx_cpp::CpuReg::P] = 
                    (carry ? fam65xx_cpp::P_CARRY : 0) |
                    (decimal ? fam65xx_cpp::P_DECIMAL : 0);
                
                uint8_t initial_p = cpu.get_registers()[fam65xx_cpp::CpuReg::P];
                
                // Execute ADC #0xd5
                cpu.get_registers()[fam65xx_cpp::pending_data] = 0xd5;
                fam65xx_cpp::AluOperations<fam65xx_cpp::DefaultBusConfig>::execute_alu_operation_fast(
                    cpu.get_registers(), fam65xx_cpp::AluOp::ADC, 0xd5);
                
                uint8_t final_a = cpu.get_registers()[fam65xx_cpp::CpuReg::A];
                uint8_t final_p = cpu.get_registers()[fam65xx_cpp::CpuReg::P];
                
                std::cout << std::hex << std::uppercase
                          << "A=" << std::setw(2) << std::setfill('0') << (int)a_val
                          << " P=" << std::setw(2) << std::setfill('0') << (int)initial_p
                          << " D=" << (int)decimal
                          << " C=" << (int)carry
                          << " -> A=" << std::setw(2) << std::setfill('0') << (int)final_a
                          << " P=" << std::setw(2) << std::setfill('0') << (int)final_p;
                
                // Analyze the flags
                bool n = final_p & fam65xx_cpp::P_NEGATIVE;
                bool v = final_p & fam65xx_cpp::P_OVERFLOW;
                bool z = final_p & fam65xx_cpp::P_ZERO;
                bool c_out = final_p & fam65xx_cpp::P_CARRY;
                
                std::cout << " [N=" << (int)n << " V=" << (int)v << " Z=" << (int)z << " C=" << (int)c_out << "]";
                
                if (decimal && (a_val == 0x78 || a_val == 0x79)) {
                    std::cout << " ***";
                }
                
                std::cout << std::endl;
            }
        }
    }
    
    return 0;
}