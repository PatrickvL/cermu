#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

int main() {
    // Test SO pin behavior with JMP instruction
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up JMP absolute instruction at address 0x1000
    uint8_t memory[65536] = {0};
    memory[0x1000] = 0x4C;  // JMP absolute
    memory[0x1001] = 0x00;  // Low byte of target address
    memory[0x1002] = 0x20;  // High byte of target address (target = 0x2000)
    
    cpu.set_pc(0x1000);
    cpu.set_p(0x24);  // Clear V flag initially
    
    std::cout << "=== SO Pin Behavior Test ===" << std::endl;
    std::cout << "Initial P register: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "V flag initially: " << ((cpu.get_p() & 0x40) ? "SET" : "CLEAR") << std::endl;
    
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
        
        // CRITICAL TEST: Set SO pin HIGH (inactive) like ProcessorTests
        bus_state |= BUS_BIT(BUS_SO_BIT);  // SO pin inactive (high)
        bus_state |= BUS_BIT(BUS_RDY_BIT); // RDY ready
        
        std::cout << "\nCycle " << cycle << ":" << std::endl;
        std::cout << "  Address: 0x" << std::hex << addr << std::endl;
        std::cout << "  SO pin state: " << ((bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH" : "LOW") << std::endl;
        std::cout << "  P before cycle: 0x" << std::hex << (int)cpu.get_p() << std::endl;
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "  P after cycle: 0x" << std::hex << (int)cpu.get_p() << std::endl;
        std::cout << "  V flag after cycle: " << ((cpu.get_p() & 0x40) ? "SET" : "CLEAR") << std::endl;
        
        // Check if instruction completed
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            break;
        }
    }
    
    std::cout << "\n=== Final State ===" << std::endl;
    std::cout << "Final PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Final P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "V flag final: " << ((cpu.get_p() & 0x40) ? "SET" : "CLEAR") << std::endl;
    
    // Test with SO pin LOW (active) to verify edge detection works
    std::cout << "\n=== Testing SO Pin LOW (Active) ===" << std::endl;
    cpu.set_pc(0x1000);
    cpu.set_p(0x24);  // Clear V flag again
    
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
        
        // CRITICAL TEST: Set SO pin LOW (active) to trigger V flag
        // bus_state |= BUS_BIT(BUS_SO_BIT);  // Comment out - SO pin low (active)
        bus_state |= BUS_BIT(BUS_RDY_BIT); // RDY ready
        
        std::cout << "\nCycle " << cycle << " (SO pin LOW):" << std::endl;
        std::cout << "  SO pin state: " << ((bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH" : "LOW") << std::endl;
        std::cout << "  P before cycle: 0x" << std::hex << (int)cpu.get_p() << std::endl;
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "  P after cycle: 0x" << std::hex << (int)cpu.get_p() << std::endl;
        std::cout << "  V flag after cycle: " << ((cpu.get_p() & 0x40) ? "SET" : "CLEAR") << std::endl;
        
        // Check if instruction completed
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            break;
        }
    }
    
    return 0;
}