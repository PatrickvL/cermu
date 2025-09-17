
#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp" 
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "ASL $nn (opcode 0x06) cycle table:" << std::endl;
    
    for (int step = 1; step <= 6; step++) {
        auto cycle = CycleTables<config_6502>::get_cycle(0x06, step);
        std::cout << "Step " << step << ": MemOp=" << (int)cycle.mem_op 
                  << " DataOp=" << (int)cycle.data_op 
                  << " AluOp=" << (int)cycle.alu_op << std::endl;
    }
    
    return 0;
}
