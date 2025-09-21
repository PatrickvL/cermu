
#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

int main() {
    using Config = config_6502;
    using TestCPU = fam65xx_cpp::fam65xx<Config>;
    
    std::cout << "=== Memory Shift/Rotate Cycle Table Analysis ===" << std::endl;
    
    uint8_t opcodes[] = {0x06, 0x16, 0x0E, 0x1E, 0x46, 0x56, 0x4E, 0x5E, 0x26, 0x36, 0x66, 0x76};
    const char* names[] = {"ASL $nn", "ASL $nn,X", "ASL $nnnn", "ASL $nnnn,X", 
                          "LSR $nn", "LSR $nn,X", "LSR $nnnn", "LSR $nnnn,X",
                          "ROL $nn", "ROL $nn,X", "ROR $nn", "ROR $nn,X"};
    
    for (int i = 0; i < 12; i++) {
        std::cout << names[i] << " (0x" << std::hex << (int)opcodes[i] << "): ";
        
        // Find the actual cycle count by checking SYNC bits
        int cycle_count = 0;
        for (int step = 1; step <= 10; step++) {
            auto cycle = TestCPU::GET_CYCLE(opcodes[i], step);
            if (cycle.is_sync()) {
                cycle_count = step;
                break;
            }
        }
        std::cout << std::dec << cycle_count << " cycles" << std::endl;
    }
    
    return 0;
}

