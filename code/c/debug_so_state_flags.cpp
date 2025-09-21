#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

int main() {
    // Test state flag management during JMP instruction
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up JMP absolute instruction at address 0x1000
    uint8_t memory[65536] = {0};
    memory[0x1000] = 0x4C;  // JMP absolute
    memory[0x1001] = 0x00;  // Low byte of target address
    memory[0x1002] = 0x20;  // High byte of target address (target = 0x2000)
    
    cpu.set_pc(0x1000);
    cpu.set_p(0x24);  // Clear V flag initially
    
    std::cout << "=== State Flag Debug Test ===" << std::endl;
    std::cout << "Initial state flags: 0x" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "Initial P register: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Execute JMP instruction cycle by cycle
    for (int cycle = 0; cycle < 3; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        bus_state_t bus_state = 0;
        if (is_write) {
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            BUS_SET_DATA(bus_state, data);
        } else {
            uint8_t data = memory[addr];
            BUS_SET_DATA(bus_state, data);
        }
        
        // Set SO pin HIGH (inactive) like ProcessorTests
        bus_state |= BUS_BIT(BUS_SO_BIT);  // SO pin inactive (high)
        bus_state |= BUS_BIT(BUS_RDY_BIT); // RDY ready
        
        std::cout << "\nCycle " << cycle << ":" << std::endl;
        std::cout << "  Before cycle_tick:" << std::endl;
        std::cout << "    State flags: 0x" << std::hex << cpu.get_state_flags() << std::endl;
        std::cout << "    P register: 0x" << std::hex << (int)cpu.get_p() << std::endl;
        std::cout << "    SO pin state: " << ((bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH" : "LOW") << std::endl;
        std::cout << "    Opcode: 0x" << std::hex << cpu.get_opcode() << std::endl;
        std::cout << "    Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "  After cycle_tick:" << std::endl;
        std::cout << "    State flags: 0x" << std::hex << cpu.get_state_flags() << std::endl;
        std::cout << "    P register: 0x" << std::hex << (int)cpu.get_p() << std::endl;
        std::cout << "    V flag: " << ((cpu.get_p() & 0x40) ? "SET" : "CLEAR") << std::endl;
        std::cout << "    Opcode: 0x" << std::hex << cpu.get_opcode() << std::endl;
        std::cout << "    Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
        
        // Check if instruction completed
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            break;
        }
    }
    
    std::cout << "\n=== Final State ===" << std::endl;
    std::cout << "Final state flags: 0x" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "Final PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Final P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "V flag final: " << ((cpu.get_p() & 0x40) ? "SET" : "CLEAR") << std::endl;
    
    return 0;
}