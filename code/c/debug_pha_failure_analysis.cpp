#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "tests/fam65xx_cpp_test_harness.cpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== PHA (0x48) Failure Analysis ===" << std::endl;
    
    // Create CPU with standard 6502 config
    using Cpu6502 = fam65xx_cpp::Family65xx<fam65xx_cpp::BusConfig6502>;
    Cpu6502 cpu;
    
    // Test a simple PHA case manually first
    std::cout << "\n--- Manual PHA Test ---" << std::endl;
    
    // Reset CPU to known state
    cpu.reset();
    
    // Set up initial state: A = 0x42, S = 0xFF
    cpu.set_reg(fam65xx_cpp::CpuReg::A, 0x42);
    cpu.set_reg(fam65xx_cpp::CpuReg::S, 0xFF);
    cpu.set_reg(fam65xx_cpp::CpuReg::P, 0x20);  // Basic status flags
    
    // Set PC to start of test
    cpu.set_reg(fam65xx_cpp::CpuReg::PCL, 0x00);
    cpu.set_reg(fam65xx_cpp::CpuReg::PCH, 0x10);
    
    // Setup memory with PHA instruction at $1000
    cpu.write_memory(0x1000, 0x48);  // PHA instruction
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  A: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.get_reg(fam65xx_cpp::CpuReg::A) << std::endl;
    std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.get_reg(fam65xx_cpp::CpuReg::S) << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
              << ((cpu.get_reg(fam65xx_cpp::CpuReg::PCH) << 8) | cpu.get_reg(fam65xx_cpp::CpuReg::PCL)) << std::endl;
    
    // Execute the PHA instruction cycle by cycle
    std::cout << "\nExecuting PHA instruction cycle by cycle:" << std::endl;
    
    for (int cycle = 1; cycle <= 3; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;
        
        // Get cycle descriptor from table
        auto cycle_desc = fam65xx_cpp::CycleTables<fam65xx_cpp::BusConfig6502>::get_cycle_from_table(0x48, cycle);
        
        std::cout << "  Cycle desc - MemOp: " << (int)cycle_desc.get_mem_op() 
                  << ", DataOp: " << (int)cycle_desc.get_data_op()
                  << ", AluOp: " << (int)cycle_desc.get_alu_op()
                  << ", Sync: " << cycle_desc.is_sync() << std::endl;
        
        // Execute the cycle
        auto old_pc = ((cpu.get_reg(fam65xx_cpp::CpuReg::PCH) << 8) | cpu.get_reg(fam65xx_cpp::CpuReg::PCL));
        auto old_s = cpu.get_reg(fam65xx_cpp::CpuReg::S);
        
        cpu.cycle_tick();
        
        auto new_pc = ((cpu.get_reg(fam65xx_cpp::CpuReg::PCH) << 8) | cpu.get_reg(fam65xx_cpp::CpuReg::PCL));
        auto new_s = cpu.get_reg(fam65xx_cpp::CpuReg::S);
        
        std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << old_pc 
                  << " -> 0x" << std::hex << std::setw(4) << std::setfill('0') << new_pc << std::endl;
        std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)old_s 
                  << " -> 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)new_s << std::endl;
        
        // Check what was written to stack
        if (cycle == 2) {  // PHA writes on cycle 2
            uint16_t stack_addr = 0x0100 | old_s;
            uint8_t stack_value = cpu.read_memory(stack_addr);
            std::cout << "  Stack[$" << std::hex << std::setw(4) << std::setfill('0') << stack_addr 
                      << "] = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)stack_value << std::endl;
        }
        
        if (cycle_desc.is_sync()) {
            std::cout << "  SYNC - instruction complete" << std::endl;
            break;
        }
    }
    
    // Final state
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  A: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.get_reg(fam65xx_cpp::CpuReg::A) << std::endl;
    std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.get_reg(fam65xx_cpp::CpuReg::S) << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
              << ((cpu.get_reg(fam65xx_cpp::CpuReg::PCH) << 8) | cpu.get_reg(fam65xx_cpp::CpuReg::PCL)) << std::endl;
    
    // Expected: A=0x42, S=0xFE, PC=0x1001, Stack[0x01FF]=0x42
    std::cout << "\nExpected results:" << std::endl;
    std::cout << "  A: 0x42 (unchanged)" << std::endl;
    std::cout << "  S: 0xFE (decremented)" << std::endl;
    std::cout << "  PC: 0x1001 (incremented)" << std::endl;
    std::cout << "  Stack[0x01FF]: 0x42 (A value pushed)" << std::endl;
    
    uint8_t final_stack_value = cpu.read_memory(0x01FF);
    std::cout << "  Actual Stack[0x01FF]: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)final_stack_value << std::endl;
    
    return 0;
}