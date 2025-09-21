
#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

int main() {
    using Config = config_6502;
    using TestCPU = fam65xx_cpp::fam65xx<Config>;
    
    std::cout << "=== Cycle Table Analysis: ASL $nn,X (0x16) ===" << std::endl;
    
    for (int step = 1; step <= 7; step++) {
        auto cycle = TestCPU::GET_CYCLE(0x16, step);
        std::cout << "Step " << step << ": MemOp=" << (int)cycle.mem_op 
                  << " DataOp=" << (int)cycle.data_op 
                  << " AluOp=" << (int)cycle.alu_op 
                  << " Sync=" << (cycle.is_sync() ? "true" : "false") << std::endl;
    }
    
    return 0;
}

