#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== Instruction Completion Debug ===" << std::endl;
    
    // Create CPU instance
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_pc(0x1000);
    cpu.set_a(0x42);
    cpu.set_s(0xFF);
    
    // Simulate PHA instruction (0x48) execution
    std::cout << "\n--- PHA (0x48) Instruction Execution ---" << std::endl;
    
    // Memory simulation: PHA at 0x1000, next instruction (NOP) at 0x1001
    uint8_t memory[65536];
    memory[0x1000] = 0x48;  // PHA
    memory[0x1001] = 0xEA;  // NOP (next instruction)
    
    bus_state_t bus_state = 0;
    
    for (int cycle = 0; cycle < 10; cycle++) {
        std::cout << "\nCycle " << cycle << ":" << std::endl;
        std::cout << "  Before: PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc();
        std::cout << ", Step=" << std::dec << (int)cpu.get_cycle_step();
        std::cout << ", Opcode=0x" << std::hex << std::setw(2) << std::setfill('0') << cpu.get_opcode() << std::endl;
        
        // Set bus data based on address
        uint16_t addr = cpu.get_address();
        uint8_t bus_data = memory[addr];
        bus_state = BUS_SET_DATA(bus_state, bus_data);
        
        std::cout << "  Memory: [0x" << std::hex << std::setw(4) << std::setfill('0') << addr;
        std::cout << "] = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)bus_data << std::endl;
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "  After:  PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc();
        std::cout << ", Step=" << std::dec << (int)cpu.get_cycle_step();
        std::cout << ", Opcode=0x" << std::hex << std::setw(2) << std::setfill('0') << cpu.get_opcode();
        
        // Check if instruction should be complete
        if (cpu.get_cycle_step() > 0) {
            const auto cycle_desc = cpu.GET_CYCLE(cpu.get_opcode(), cpu.get_cycle_step());
            std::cout << ", SYNC=" << (cycle_desc.is_sync() ? "YES" : "NO");
        }
        std::cout << std::endl;
        
        // Stop if we've completed the PHA and started fetching next instruction
        if (cycle > 0 && cpu.get_opcode() == 0xEA) {
            std::cout << "\nNext instruction (NOP) started - PHA completed successfully!" << std::endl;
            break;
        }
        
        // Safety check for infinite loops
        if (cycle > 0 && cpu.get_opcode() == 0x48 && cpu.get_cycle_step() == 0) {
            std::cout << "\nWARNING: PHA instruction restarted - infinite loop detected!" << std::endl;
            break;
        }
    }
    
    return 0;
}