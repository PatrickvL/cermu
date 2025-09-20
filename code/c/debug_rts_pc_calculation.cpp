#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== RTS PC Calculation Debug Test ===" << std::endl;
    
    // Create CPU instance  
    fam65xx<cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, 16, false, false, false>> cpu;
    cpu.init_for_test();
    
    // Set up a simple test scenario
    // Put RTS at address 0x4000
    cpu.set_pc(0x4000);
    
    // Set up stack with a known return address
    // Let's say we want to return to 0x1234
    // JSR pushes PC-1, so stack should contain 0x1233
    cpu.set_sp(0xFF);  // Start with full stack
    
    // Manual stack setup: push 0x1233 onto stack
    // Stack grows downward from 0x01FF
    // First push high byte (0x12), then low byte (0x33)
    cpu.set_sp(0xFD);  // After two pushes
    
    std::cout << "Initial setup:" << std::endl;
    std::cout << "PC: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "SP: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_sp() << std::endl;
    
    // Create a memory model for stack data
    uint8_t memory[0x10000] = {0};
    
    // Set up RTS opcode at 0x4000
    memory[0x4000] = 0x60;  // RTS
    
    // Set up stack memory
    // Stack addresses: 0x01FE = high byte, 0x01FF = low byte  
    memory[0x01FE] = 0x12;  // High byte of return address
    memory[0x01FF] = 0x33;  // Low byte of return address
    
    // Execute RTS instruction cycle by cycle
    bus_state_t bus_state = 0;
    
    std::cout << "\n=== Cycle-by-cycle RTS execution ===" << std::endl;
    
    // Cycle 0: Opcode fetch
    std::cout << "\nCycle 0: Opcode fetch" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, memory[cpu.get_address()]);
    std::cout << "Address: 0x" << std::hex << std::setw(4) << cpu.get_address() << std::endl;
    std::cout << "Data provided: 0x" << std::hex << std::setw(2) << (int)BUS_GET_DATA(bus_state) << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    std::cout << "After cycle 0 - PC: 0x" << std::hex << std::setw(4) << cpu.get_pc() 
              << ", SP: 0x" << std::hex << std::setw(2) << (int)cpu.get_sp() << std::endl;
    
    // Cycles 1-6: RTS execution
    for (int cycle = 1; cycle <= 6; cycle++) {
        std::cout << "\nCycle " << cycle << ":" << std::endl;
        
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        std::cout << "Address: 0x" << std::hex << std::setw(4) << addr;
        std::cout << ", R/W: " << (is_read ? "READ" : "WRITE") << std::endl;
        
        if (is_read) {
            // Provide data from memory
            bus_state = BUS_SET_DATA(bus_state, memory[addr]);
            std::cout << "Data provided: 0x" << std::hex << std::setw(2) << (int)memory[addr] << std::endl;
        } else {
            std::cout << "Write data: 0x" << std::hex << std::setw(2) << (int)cpu.get_write_data() << std::endl;
        }
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "After cycle " << cycle << " - PC: 0x" << std::hex << std::setw(4) << cpu.get_pc() 
                  << ", SP: 0x" << std::hex << std::setw(2) << (int)cpu.get_sp()
                  << ", Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
                  
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0) {
            std::cout << "Instruction completed after cycle " << cycle << std::endl;
            break;
        }
    }
    
    std::cout << "\n=== Final Results ===" << std::endl;
    std::cout << "Final PC: 0x" << std::hex << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "Expected PC: 0x1234 (0x1233 + 1)" << std::endl;
    std::cout << "Final SP: 0x" << std::hex << std::setw(2) << (int)cpu.get_sp() << std::endl;
    std::cout << "Expected SP: 0xFF (after pulling 2 bytes)" << std::endl;
    
    // Check if PC calculation is correct
    bool pc_correct = (cpu.get_pc() == 0x1234);
    bool sp_correct = (cpu.get_sp() == 0xFF);
    
    std::cout << "\nResults:" << std::endl;
    std::cout << (pc_correct ? "✅" : "❌") << " PC calculation" << std::endl;
    std::cout << (sp_correct ? "✅" : "❌") << " SP calculation" << std::endl;
    
    return 0;
}