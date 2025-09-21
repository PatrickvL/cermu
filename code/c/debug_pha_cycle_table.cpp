#include "src/chip/cpu/fam65xx_cpp/cycle_table_gen.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== PHA (0x48) Cycle Table Debug ===" << std::endl;
    
    // Test the cycle table directly
    using Config = config_6502;
    using CycleTables = fam65xx_cpp::CycleTables<Config>;
    
    std::cout << "\nTesting PHA cycle table entries:" << std::endl;
    
    for (uint8_t cycle = 1; cycle <= 5; cycle++) {
        auto cycle_desc = CycleTables::get_cycle_from_table(0x48, cycle);
        
        std::cout << "Cycle " << (int)cycle << ":" << std::endl;
        std::cout << "  MemOp: " << (int)cycle_desc.get_mem_op() << std::endl;
        std::cout << "  DataOp: " << (int)cycle_desc.get_data_op() << std::endl;
        std::cout << "  AluOp: " << (int)cycle_desc.get_alu_op() << std::endl;
        std::cout << "  Sync: " << cycle_desc.is_sync() << std::endl;
        
        // Check if this is an empty cycle
        if (cycle_desc.get_mem_op() == MemOp::NOP &&
            cycle_desc.get_data_op() == DataOp::NOP &&
            cycle_desc.get_alu_op() == AluOp::NOP &&
            !cycle_desc.is_sync()) {
            std::cout << "  -> Empty cycle (instruction should have ended)" << std::endl;
            break;
        }
        
        if (cycle_desc.is_sync()) {
            std::cout << "  -> SYNC found - instruction should complete" << std::endl;
            break;
        }
    }
    
    // Test the template-based lookup as well
    std::cout << "\nTesting template-based lookup:" << std::endl;
    auto cycle1 = CycleTables::get_cycle<0x48, 1>();
    auto cycle2 = CycleTables::get_cycle<0x48, 2>();
    auto cycle3 = CycleTables::get_cycle<0x48, 3>();
    
    std::cout << "Template Cycle 1 - MemOp:" << (int)cycle1.get_mem_op() 
              << " DataOp:" << (int)cycle1.get_data_op() 
              << " Sync:" << cycle1.is_sync() << std::endl;
    std::cout << "Template Cycle 2 - MemOp:" << (int)cycle2.get_mem_op() 
              << " DataOp:" << (int)cycle2.get_data_op() 
              << " Sync:" << cycle2.is_sync() << std::endl;
    std::cout << "Template Cycle 3 - MemOp:" << (int)cycle3.get_mem_op() 
              << " DataOp:" << (int)cycle3.get_data_op() 
              << " Sync:" << cycle3.is_sync() << std::endl;
    
    // Check enum values
    std::cout << "\nEnum value verification:" << std::endl;
    std::cout << "MemOp::READ_PC = " << (int)MemOp::READ_PC << std::endl;
    std::cout << "MemOp::WRITE_SP_DEC = " << (int)MemOp::WRITE_SP_DEC << std::endl;
    std::cout << "DataOp::NOP = " << (int)DataOp::NOP << std::endl;
    std::cout << "DataOp::STORE_A = " << (int)DataOp::STORE_A << std::endl;
    std::cout << "AluOp::NOP = " << (int)AluOp::NOP << std::endl;
    
    return 0;
}