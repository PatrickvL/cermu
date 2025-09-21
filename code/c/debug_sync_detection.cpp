#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== SYNC Flag Detection Debug ===" << std::endl;
    
    // Test PHA (0x48) SYNC detection
    std::cout << "\n--- PHA (0x48) Cycle Table Analysis ---" << std::endl;
    
    for (uint8_t cycle = 0; cycle <= 5; cycle++) {
        auto cycle_desc = fam65xx<config_6502>::GET_CYCLE(0x48, cycle);
        
        std::cout << "Cycle " << static_cast<int>(cycle)
                  << ": MemOp=" << static_cast<int>(cycle_desc.mem_op)
                  << ", DataOp=" << static_cast<int>(cycle_desc.data_op)
                  << ", AluOp=" << static_cast<int>(cycle_desc.alu_op)
                  << ", SYNC=" << (cycle_desc.is_sync() ? "YES" : "NO")
                  << std::endl;
    }
    
    // Test PLA (0x68) SYNC detection
    std::cout << "\n--- PLA (0x68) Cycle Table Analysis ---" << std::endl;
    
    for (uint8_t cycle = 0; cycle <= 5; cycle++) {
        auto cycle_desc = fam65xx<config_6502>::GET_CYCLE(0x68, cycle);
        
        std::cout << "Cycle " << static_cast<int>(cycle)
                  << ": MemOp=" << static_cast<int>(cycle_desc.mem_op)
                  << ", DataOp=" << static_cast<int>(cycle_desc.data_op)
                  << ", AluOp=" << static_cast<int>(cycle_desc.alu_op)
                  << ", SYNC=" << (cycle_desc.is_sync() ? "YES" : "NO")
                  << std::endl;
    }
    
    // Test cycle_desc_t SYNC bit manipulation directly
    std::cout << "\n--- SYNC Bit Manipulation Test ---" << std::endl;
    
    // Create a cycle with SYNC manually
    auto sync_cycle = CD_MAKE_SYNC(MemOp::WRITE_SP_DEC, DataOp::STORE_A, AluOp::NOP);
    auto no_sync_cycle = CD_MAKE(MemOp::WRITE_SP_DEC, DataOp::STORE_A, AluOp::NOP);
    
    std::cout << "Manual SYNC cycle: SYNC=" << (sync_cycle.is_sync() ? "YES" : "NO") << std::endl;
    std::cout << "Manual NO-SYNC cycle: SYNC=" << (no_sync_cycle.is_sync() ? "YES" : "NO") << std::endl;
    
    return 0;
}