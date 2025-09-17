#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

int main() {
    using namespace fam65xx_cpp;
    using config = config_6502;
    using cpu_type = fam65xx<config>;
    
    // Check ASL $nn opcode 0x06 cycle pattern
    std::cout << "ASL $nn (opcode 0x06) cycle pattern:" << std::endl;
    for (int cycle = 1; cycle <= 6; cycle++) {
        auto desc = cpu_type::GET_CYCLE(0x06, cycle);
        std::cout << "  Cycle " << cycle 
                  << ": MemOp=" << static_cast<int>(desc.mem_op)
                  << " DataOp=" << static_cast<int>(desc.data_op)
                  << " AluOp=" << static_cast<int>(desc.alu_op)
                  << " Sync=" << (desc.is_sync() ? "YES" : "NO")
                  << std::endl;
    }
    
    std::cout << std::endl << "Enum values for reference:" << std::endl;
    std::cout << "DataOp::TEMP_STORE=" << static_cast<int>(DataOp::TEMP_STORE) << std::endl;
    std::cout << "DataOp::TEMP_MODIFY=" << static_cast<int>(DataOp::TEMP_MODIFY) << std::endl;
    std::cout << "MemOp::READ_ZP=" << static_cast<int>(MemOp::READ_ZP) << std::endl;
    std::cout << "MemOp::WRITE_ZP=" << static_cast<int>(MemOp::WRITE_ZP) << std::endl;
    
    return 0;
}
