#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>

int main() {
    using Config = config_6502;
    using CPU = fam65xx_cpp::fam65xx<Config>;
    
    // Check what cycle data is generated for LDA immediate (0xa9)
    std::cout << "=== LDA #$nn (0xa9) Cycle Analysis ===" << std::endl;
    
    for (uint8_t step = 1; step <= 3; step++) {
        auto cycle = CPU::GET_CYCLE(0xa9, step);
        std::cout << "Step " << (int)step << ":" << std::endl;
        std::cout << "  MemOp: " << (int)cycle.mem_op << std::endl;
        std::cout << "  DataOp: " << (int)cycle.data_op << std::endl;
        std::cout << "  AluOp: " << (int)cycle.alu_op << std::endl;
        std::cout << "  Sync: " << (cycle.is_sync() ? "YES" : "NO") << std::endl;
    }
    
    // Also check the DataOp enum values
    std::cout << "\n=== DataOp Enum Values ===" << std::endl;
    std::cout << "LOAD_A = " << (int)DataOp::LOAD_A << std::endl;
    std::cout << "LOAD_X = " << (int)DataOp::LOAD_X << std::endl;
    std::cout << "LOAD_Y = " << (int)DataOp::LOAD_Y << std::endl;
    std::cout << "ALU = " << (int)DataOp::ALU << std::endl;
    
    // And CpuReg enum values
    std::cout << "\n=== CpuReg Enum Values ===" << std::endl;
    std::cout << "A = " << (int)CpuReg::A << std::endl;
    std::cout << "X = " << (int)CpuReg::X << std::endl;
    std::cout << "Y = " << (int)CpuReg::Y << std::endl;
    std::cout << "P = " << (int)CpuReg::P << std::endl;
    
    return 0;
}