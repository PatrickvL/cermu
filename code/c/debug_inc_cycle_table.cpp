#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

int main() {
    std::cout << "=== INC vs ROR Cycle Table Comparison ===" << std::endl;
    
    // Compare INC $nn (0xE6) vs ROR $nn (0x66)
    std::cout << "\nINC $nn (0xE6) cycle table:" << std::endl;
    for (int cycle = 1; cycle <= 6; cycle++) {
        auto desc = TestCPU::GET_CYCLE(0xE6, cycle);
        std::cout << "Cycle " << cycle << ": mem_op=" << (int)desc.mem_op 
                  << ", data_op=" << (int)desc.data_op 
                  << ", alu_op=" << (int)desc.alu_op 
                  << ", sync=" << (desc.is_sync() ? "yes" : "no") << std::endl;
    }
    
    std::cout << "\nROR $nn (0x66) cycle table:" << std::endl;
    for (int cycle = 1; cycle <= 6; cycle++) {
        auto desc = TestCPU::GET_CYCLE(0x66, cycle);
        std::cout << "Cycle " << cycle << ": mem_op=" << (int)desc.mem_op 
                  << ", data_op=" << (int)desc.data_op 
                  << ", alu_op=" << (int)desc.alu_op 
                  << ", sync=" << (desc.is_sync() ? "yes" : "no") << std::endl;
    }
    
    return 0;
}