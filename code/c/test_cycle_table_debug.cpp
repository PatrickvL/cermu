#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/cycle_table_gen.hpp"

using TestConfig = config_6502;
using namespace fam65xx_cpp;

int main() {
    std::cout << "=== ASL $nn (0x06) Cycle Table Debug ===" << std::endl;
    
    using CycleTables = fam65xx_cpp::CycleTables<TestConfig>;
    
    std::cout << "Checking cycle table for opcode 0x06 (ASL $nn):\n" << std::endl;
    
    for (uint8_t cycle = 1; cycle <= 10; cycle++) {
        auto desc = CycleTables::get_cycle_from_table(0x06, cycle);
        
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