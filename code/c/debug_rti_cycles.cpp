#include <iostream>
#include <cstdio>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

int main() {
    printf("=== RTI CYCLE DEFINITIONS DEBUG ===\n\n");
    
    // Create CPU instance to access cycle definitions
    fam65xx<config_6502> cpu;
    
    // Check RTI cycle definitions
    for (int cycle = 1; cycle <= 6; cycle++) {
        auto desc = cpu.GET_CYCLE(0x40, cycle);
        printf("RTI Cycle %d: MemOp=%d, DataOp=%d, AluOp=%d, Sync=%s\n",
               cycle, desc.mem_op, desc.data_op, desc.alu_op,
               desc.is_sync() ? "YES" : "NO");
    }
    
    printf("\n=== COMPARISON: RTS CYCLE DEFINITIONS ===\n\n");
    
    // Compare with RTS cycle definitions
    for (int cycle = 1; cycle <= 6; cycle++) {
        auto desc = cpu.GET_CYCLE(0x60, cycle);
        printf("RTS Cycle %d: MemOp=%d, DataOp=%d, AluOp=%d, Sync=%s\n",
               cycle, desc.mem_op, desc.data_op, desc.alu_op,
               desc.is_sync() ? "YES" : "NO");
    }
    
    return 0;
}
