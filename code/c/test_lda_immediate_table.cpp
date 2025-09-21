#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/cycle_table_gen.hpp"

using TestConfig = config_6502;
using namespace fam65xx_cpp;

int main() {
    std::cout << "=== LDA #imm (0xA9) Cycle Table Debug ===" << std::endl;
    
    using CycleTables = fam65xx_cpp::CycleTables<TestConfig>;
    
    std::cout << "Checking cycle table for opcode 0xA9 (LDA #imm):\n" << std::endl;
    
    for (uint8_t cycle = 1; cycle <= 5; cycle++) {
        auto desc = CycleTables::get_cycle_from_table(0xA9, cycle);
        
        std::cout << "Cycle " << (int)cycle << ": ";
        std::cout << "MemOp=" << (int)desc.get_mem_op() << " ";
        std::cout << "DataOp=" << (int)desc.get_data_op() << " ";
        std::cout << "AluOp=" << (int)desc.get_alu_op() << " ";
        std::cout << "Sync=" << (desc.is_sync() ? "true" : "false");
        std::cout << std::endl;
        
        // Stop at sync cycle
        if (desc.is_sync()) {
            break;
        }
    }
    
    return 0;
}