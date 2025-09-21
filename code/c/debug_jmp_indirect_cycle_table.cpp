#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <cstdio>

using namespace fam65xx_cpp;

int main() {
    printf("=== JMP INDIRECT CYCLE TABLE DEBUG ===\n");
    
    // Use config_6502 for NMOS processor
    using cpu_t = fam65xx<config_6502>;
    
    // Test cycle table lookup for JMP indirect (0x6C)
    uint16_t opcode = 0x6C;
    
    printf("Testing JMP indirect (0x6C) cycle definitions:\n");
    
    for (uint8_t cycle = 1; cycle <= 8; cycle++) {
        auto cycle_desc = cpu_t::GET_CYCLE(opcode, cycle);
        
        printf("Cycle %d: MemOp=%d, DataOp=%d, AluOp=%d, SYNC=%s\n",
               cycle,
               static_cast<int>(cycle_desc.get_mem_op()),
               static_cast<int>(cycle_desc.get_data_op()),
               static_cast<int>(cycle_desc.get_alu_op()),
               cycle_desc.is_sync() ? "YES" : "NO");
        
        // If this is an empty cycle, we've reached the end
        if (cycle_desc.get_mem_op() == MemOp::NOP &&
            cycle_desc.get_data_op() == DataOp::NOP &&
            cycle_desc.get_alu_op() == AluOp::NOP &&
            !cycle_desc.is_sync()) {
            printf("  -> Empty cycle detected, instruction ends here\n");
            break;
        }
        
        if (cycle_desc.is_sync()) {
            printf("  -> SYNC flag detected, instruction should complete\n");
            break;
        }
    }
    
    return 0;
}