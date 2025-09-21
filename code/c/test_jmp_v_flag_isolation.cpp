#include <iostream>
#include <cassert>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

int main() {
    std::cout << "=== JMP V Flag Isolation Test ===" << std::endl;
    
    // Create CPU instance
    fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
    cpu.init_for_test();
    
    // Test 1: Set V flag initially, verify JMP doesn't clear it
    std::cout << "\nTest 1: V flag initially SET" << std::endl;
    cpu.set_pc(0x1000);
    cpu.set_status(0x66); // V flag SET (0x40), plus other flags
    
    // Set up memory for JMP $2000
    // 0x1000: 0x4C (JMP absolute)
    // 0x1001: 0x00 (low byte of target)
    // 0x1002: 0x20 (high byte of target)
    uint8_t memory[65536] = {0};
    memory[0x1000] = 0x4C;
    memory[0x1001] = 0x00;
    memory[0x1002] = 0x20;
    
    std::cout << "Initial P: 0x" << std::hex << (int)cpu.get_status() << std::dec << std::endl;
    std::cout << "Initial PC: 0x" << std::hex << cpu.get_pc() << std::dec << std::endl;
    
    // Execute JMP instruction cycle by cycle with SO pin HIGH (inactive)
    for (int cycle = 0; cycle < 4; cycle++) {
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
        
        // CRITICAL: Set SO pin HIGH (inactive) like ProcessorTests
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        bus_state |= BUS_BIT(BUS_SO_BIT);  // SO pin inactive (high)
        
        std::cout << "Cycle " << cycle << ": addr=0x" << std::hex << addr << ", data=0x" << (int)BUS_GET_DATA(bus_state) << std::dec;
        std::cout << ", P_before=0x" << std::hex << (int)cpu.get_status() << std::dec;
        
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << ", P_after=0x" << std::hex << (int)cpu.get_status() << std::dec << std::endl;
        
        if (cpu.get_cycle_step() == 0) {
            std::cout << "Instruction completed after cycle " << cycle << std::endl;
            break;
        }
    }
    
    std::cout << "Final P: 0x" << std::hex << (int)cpu.get_status() << std::dec << std::endl;
    std::cout << "Final PC: 0x" << std::hex << cpu.get_pc() << std::dec << std::endl;
    
    // Verify V flag is still set (unchanged)
    if ((cpu.get_status() & 0x40) != 0) {
        std::cout << "✅ SUCCESS: V flag preserved (JMP doesn't affect flags)" << std::endl;
    } else {
        std::cout << "❌ FAILURE: V flag was cleared (JMP incorrectly affects flags)" << std::endl;
        return 1;
    }
    
    // Test 2: V flag initially CLEAR, verify JMP doesn't set it
    std::cout << "\nTest 2: V flag initially CLEAR" << std::endl;
    cpu.set_pc(0x1000);
    cpu.set_status(0x26); // V flag CLEAR (no 0x40), other flags same
    
    std::cout << "Initial P: 0x" << std::hex << (int)cpu.get_status() << std::dec << std::endl;
    
    // Execute JMP instruction again with same setup
    for (int cycle = 0; cycle < 4; cycle++) {
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
        
        // CRITICAL: Set SO pin HIGH (inactive) like ProcessorTests
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        bus_state |= BUS_BIT(BUS_SO_BIT);  // SO pin inactive (high)
        
        std::cout << "Cycle " << cycle << ": P_before=0x" << std::hex << (int)cpu.get_status() << std::dec;
        
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << ", P_after=0x" << std::hex << (int)cpu.get_status() << std::dec << std::endl;
        
        if (cpu.get_cycle_step() == 0) {
            break;
        }
    }
    
    std::cout << "Final P: 0x" << std::hex << (int)cpu.get_status() << std::dec << std::endl;
    
    // Verify V flag is still clear (unchanged)
    if ((cpu.get_status() & 0x40) == 0) {
        std::cout << "✅ SUCCESS: V flag preserved (JMP doesn't affect flags)" << std::endl;
    } else {
        std::cout << "❌ FAILURE: V flag was set (JMP incorrectly affects flags)" << std::endl;
        return 1;
    }
    
    std::cout << "\n🎉 ALL TESTS PASSED: JMP preserves V flag correctly!" << std::endl;
    return 0;
}