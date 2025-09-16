#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>

using namespace fam65xx_cpp;

int main() {
    // Test the cycle table lookup for ASL A (opcode 0x0A)
    constexpr uint16_t opcode = 0x0A;
    
    std::cout << "=== ASL A (0x0A) Cycle Table Debug ===" << std::endl;
    
    // Check cycles 1 through 3
    for (uint8_t cycle = 1; cycle <= 3; cycle++) {
        auto cycle_desc = CycleTables<config_6502>::get_cycle(opcode, cycle);
        
        std::cout << "Cycle " << (int)cycle << ":" << std::endl;
        std::cout << "  MemOp: " << (int)cycle_desc.mem_op << " (";
        
        switch (static_cast<MemOp>(cycle_desc.mem_op)) {
            case MemOp::NOP: std::cout << "NOP"; break;
            case MemOp::READ_PC_INC: std::cout << "READ_PC_INC"; break;
            case MemOp::READ_PC: std::cout << "READ_PC"; break;
            case MemOp::READ_ABS: std::cout << "READ_ABS"; break;
            default: std::cout << "UNKNOWN"; break;
        }
        
        std::cout << ")" << std::endl;
        std::cout << "  DataOp: " << (int)cycle_desc.data_op << " (";
        
        switch (static_cast<DataOp>(cycle_desc.data_op)) {
            case DataOp::NOP: std::cout << "NOP"; break;
            case DataOp::ALU: std::cout << "ALU"; break;
            case DataOp::LOAD_A: std::cout << "LOAD_A"; break;
            default: std::cout << "UNKNOWN"; break;
        }
        
        std::cout << ")" << std::endl;
        std::cout << "  AluOp: " << (int)cycle_desc.alu_op << " (";
        
        switch (static_cast<AluOp>(cycle_desc.alu_op)) {
            case AluOp::NOP: std::cout << "NOP"; break;
            case AluOp::ASL: std::cout << "ASL"; break;
            default: std::cout << "UNKNOWN"; break;
        }
        
        std::cout << ")" << std::endl;
        std::cout << "  Sync: " << (cycle_desc.is_sync() ? "true" : "false") << std::endl;
        std::cout << std::endl;
        
        // Stop at first empty cycle
        if (cycle_desc.mem_op == 0 && cycle_desc.data_op == 0 && cycle_desc.alu_op == 0 && !cycle_desc.is_sync()) {
            break;
        }
    }
    
    return 0;
}