#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== PHA (0x48) CRITICAL ISSUES DEBUG ===" << std::endl;
    
    // Create CPU exactly like ProcessorTests
    fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];
    std::fill(memory, memory + 65536, 0);
    
    // Initialize CPU for testing
    cpu.init_for_test();
    
    // Test case from ProcessorTests failure: "48 e0 36"
    // Set up the failing test case
    cpu.set_pc(0x5063);      // Initial PC
    cpu.set_a(0x3e);         // A register contains 0x3e (this should be pushed)
    cpu.set_sp(0x9f);        // Stack pointer at 0x9f (so push goes to 0x01+0x9f = 0x19f)
    cpu.set_status(0x36);    // Status register
    
    // Set up memory for PHA instruction
    memory[0x5063] = 0x48;   // PHA opcode
    
    std::cout << "\n=== INITIAL STATE ===" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "SP: 0x" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "Memory[0x19f]: 0x" << std::hex << (int)memory[0x19f] << std::endl;
    
    std::cout << "\n=== EXECUTING PHA INSTRUCTION ===" << std::endl;
    
    // Execute instruction using ProcessorTests-style execution
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        bus_state_t bus_state = 0;
        
        if (is_write) {
            // Write cycle - this is where the stack push should happen
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            BUS_SET_DATA(bus_state, data);
            
            std::cout << "Cycle " << cycle << ": WRITE addr=0x" << std::hex << addr 
                      << " data=0x" << std::hex << (int)data << " (A=0x" << (int)cpu.get_a() << ")" << std::endl;
        } else {
            // Read cycle
            uint8_t data = memory[addr];
            BUS_SET_DATA(bus_state, data);
            
            std::cout << "Cycle " << cycle << ": READ  addr=0x" << std::hex << addr 
                      << " data=0x" << std::hex << (int)data << std::endl;
        }
        
        // Set RDY line (ready)
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction completed
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            std::cout << "Instruction completed at cycle " << cycle << std::endl;
            break;
        }
    }
    
    std::cout << "\n=== FINAL STATE ===" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << " (expected: 0x5064)" << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "SP: 0x" << std::hex << (int)cpu.get_sp() << " (expected: 0x9e)" << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "Memory[0x19f]: 0x" << std::hex << (int)memory[0x19f] << " (expected: 0x3e)" << std::endl;
    
    std::cout << "\n=== ANALYSIS ===" << std::endl;
    if (cpu.get_pc() != 0x5064) {
        std::cout << "❌ PC ISSUE: Advanced to 0x" << std::hex << cpu.get_pc() 
                  << " instead of 0x5064" << std::endl;
    }
    if (memory[0x19f] != 0x3e) {
        std::cout << "❌ MEMORY ISSUE: Wrote 0x" << std::hex << (int)memory[0x19f] 
                  << " instead of 0x3e to stack" << std::endl;
    }
    if (cpu.get_sp() != 0x9e) {
        std::cout << "❌ STACK POINTER ISSUE: SP is 0x" << std::hex << (int)cpu.get_sp() 
                  << " instead of 0x9e" << std::endl;
    }
    
    return 0;
}