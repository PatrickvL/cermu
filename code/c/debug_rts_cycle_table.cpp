#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

using namespace fam65xx_cpp;
using BusConfig = cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, 16, false, false, false>;
using Tables = fam65xx_cpp::CycleTables<BusConfig>;

int main() {
    std::cout << "=== RTS Cycle Table Debug Test ===\n\n";
    
    std::cout << "Checking RTS (0x60) cycle definitions from cycle table:\n\n";
    
    for (uint8_t cycle = 1; cycle <= 8; ++cycle) {
        auto cycle_desc = Tables::get_cycle(0x60, cycle);
        
        std::cout << "Cycle " << static_cast<int>(cycle) << ": ";
        std::cout << "MemOp=" << static_cast<int>(cycle_desc.get_mem_op()) << ", ";
        std::cout << "DataOp=" << static_cast<int>(cycle_desc.get_data_op()) << ", ";
        std::cout << "AluOp=" << static_cast<int>(cycle_desc.get_alu_op()) << ", ";
        std::cout << "Sync=" << (cycle_desc.is_sync() ? "YES" : "NO");
        
        // Check if cycle is empty (NOP/NOP/NOP/NoSync)
        bool is_empty = (cycle_desc.get_mem_op() == MemOp::NOP &&
                        cycle_desc.get_data_op() == DataOp::NOP &&
                        cycle_desc.get_alu_op() == AluOp::NOP &&
                        !cycle_desc.is_sync());
        
        if (is_empty) {
            std::cout << " (EMPTY - RTS should complete before this)";
        }
        
        std::cout << "\n";
        
        // Stop at first empty cycle
        if (is_empty) break;
    }
    
    std::cout << "\n=== Memory Operation Analysis ===\n";
    std::cout << "Expected RTS behavior:\n";
    std::cout << "Cycle 1: READ_PC (no increment) - read dummy byte\n";
    std::cout << "Cycle 2: READ_SP - internal operation\n";
    std::cout << "Cycle 3: READ_SP_INC + STACK_PULL - pull PCL\n";
    std::cout << "Cycle 4: READ_SP_INC + STACK_PULL - pull PCH\n";
    std::cout << "Cycle 5: READ_PC - internal PC increment\n";
    std::cout << "Cycle 6: NOP + SYNC - complete instruction\n";
    
    return 0;
}