#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== Cycle Step Advancement Debug ===" << std::endl;
    
    // Create CPU instance
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_pc(0x1000);
    cpu.set_a(0x42);
    cpu.set_s(0xFF);
    
    // Memory simulation: PHA at 0x1000
    uint8_t memory[65536];
    memory[0x1000] = 0x48;  // PHA
    memory[0x1001] = 0xEA;  // NOP (next instruction)
    
    bus_state_t bus_state = 0;
    
    std::cout << "\n--- Executing PHA Instruction Step by Step ---" << std::endl;
    
    // Cycle 0: Opcode fetch
    std::cout << "\n=== CYCLE 0: Opcode Fetch ===" << std::endl;
    std::cout << "Before: PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc()
              << ", Step=" << std::dec << (int)cpu.get_cycle_step() 
              << ", Opcode=0x" << std::hex << std::setw(2) << std::setfill('0') << cpu.get_opcode() << std::endl;
              
    uint16_t addr = cpu.get_address();
    uint8_t bus_data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, bus_data);
    std::cout << "Memory read: [0x" << std::hex << std::setw(4) << std::setfill('0') << addr << "] = 0x" 
              << std::hex << std::setw(2) << std::setfill('0') << (int)bus_data << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After:  PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc()
              << ", Step=" << std::dec << (int)cpu.get_cycle_step() 
              << ", Opcode=0x" << std::hex << std::setw(2) << std::setfill('0') << cpu.get_opcode() << std::endl;
    
    // Cycle 1: Should advance to step 2
    std::cout << "\n=== CYCLE 1: Should advance from Step 1 to Step 2 ===" << std::endl;
    std::cout << "Before: Step=" << std::dec << (int)cpu.get_cycle_step() << std::endl;
    
    // Get cycle descriptor for step 1
    auto cycle_desc_1 = cpu.GET_CYCLE(cpu.get_opcode(), cpu.get_cycle_step());
    std::cout << "Cycle 1 descriptor: MemOp=" << (int)cycle_desc_1.mem_op 
              << ", DataOp=" << (int)cycle_desc_1.data_op
              << ", AluOp=" << (int)cycle_desc_1.alu_op
              << ", SYNC=" << (cycle_desc_1.is_sync() ? "YES" : "NO") << std::endl;
    
    addr = cpu.get_address();
    bus_data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, bus_data);
    std::cout << "Memory access: [0x" << std::hex << std::setw(4) << std::setfill('0') << addr << "] = 0x" 
              << std::hex << std::setw(2) << std::setfill('0') << (int)bus_data << std::endl;
    
    // Check if instruction should complete on this cycle
    std::cout << "instruction_complete should be: " << (cycle_desc_1.is_sync() ? "true" : "false") << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After:  Step=" << std::dec << (int)cpu.get_cycle_step() << std::endl;
    
    if (cpu.get_cycle_step() == 1) {
        std::cout << "ERROR: Step did not advance! Stuck at step 1!" << std::endl;
    } else {
        std::cout << "SUCCESS: Step advanced to " << (int)cpu.get_cycle_step() << std::endl;
    }
    
    // Try one more cycle
    std::cout << "\n=== CYCLE 2: Final check ===" << std::endl;
    std::cout << "Before: Step=" << std::dec << (int)cpu.get_cycle_step() << std::endl;
    
    if (cpu.get_cycle_step() > 0) {
        auto cycle_desc_2 = cpu.GET_CYCLE(cpu.get_opcode(), cpu.get_cycle_step());
        std::cout << "Cycle 2 descriptor: MemOp=" << (int)cycle_desc_2.mem_op 
                  << ", DataOp=" << (int)cycle_desc_2.data_op
                  << ", AluOp=" << (int)cycle_desc_2.alu_op
                  << ", SYNC=" << (cycle_desc_2.is_sync() ? "YES" : "NO") << std::endl;
    }
    
    addr = cpu.get_address();
    bus_data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, bus_data);
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After:  Step=" << std::dec << (int)cpu.get_cycle_step() << std::endl;
    
    return 0;
}