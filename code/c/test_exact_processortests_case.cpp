#include <iostream>
#include <cassert>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

int main() {
    std::cout << "=== Exact ProcessorTests Case Test ===" << std::endl;
    
    // Use the exact same failing test case: "4c 2a cd"
    // This was one of the first failing cases in the ProcessorTests output
    std::cout << "\nReproducing exact failing test case: '4c 2a cd'" << std::endl;
    
    // Create CPU instance (same as ProcessorTests)
    fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up the exact same initial state as the failing test
    // From ProcessorTests output: PC=0x3b72, expected P=0x26, got P=0x66
    cpu.set_pc(0x3b72);
    cpu.set_status(0x26); // Expected final status
    
    // Set up memory for the exact same instruction and target
    uint8_t memory[65536] = {0};
    memory[0x3b72] = 0x4C; // JMP absolute
    memory[0x3b73] = 0x2A; // Low byte of target address
    memory[0x3b74] = 0xCD; // High byte of target address
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::dec << std::endl;
    std::cout << "  P: 0x" << std::hex << (int)cpu.get_status() << std::dec << std::endl;
    
    // Execute JMP instruction cycle by cycle with EXACT ProcessorTests environment
    std::cout << "\nExecuting cycles:" << std::endl;
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
        
        // EXACT ProcessorTests setup
        bus_state |= BUS_BIT(BUS_RDY_BIT);      // RDY line ready
        bus_state |= BUS_BIT(BUS_SO_BIT);       // SO pin inactive (high)
        
        std::cout << "Cycle " << cycle << ":" << std::endl;
        std::cout << "  addr=0x" << std::hex << addr << ", data=0x" << (int)BUS_GET_DATA(bus_state) << std::dec;
        std::cout << ", rw=" << (is_write ? "W" : "R") << std::endl;
        std::cout << "  bus_state=0x" << std::hex << bus_state << std::dec;
        std::cout << ", SO_pin=" << ((bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH" : "LOW") << std::endl;
        std::cout << "  P_before=0x" << std::hex << (int)cpu.get_status() << std::dec;
        
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << ", P_after=0x" << std::hex << (int)cpu.get_status() << std::dec << std::endl;
        std::cout << "  PC_after=0x" << std::hex << cpu.get_pc() << std::dec << std::endl;
        
        if (cpu.get_cycle_step() == 0) {
            std::cout << "Instruction completed after cycle " << cycle << std::endl;
            break;
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << " (expected: 0xcd2a)" << std::dec << std::endl;
    std::cout << "  P: 0x" << std::hex << (int)cpu.get_status() << " (expected: 0x26)" << std::dec << std::endl;
    
    // Check if we reproduce the same failure
    uint8_t final_p = cpu.get_status();
    uint8_t expected_p = 0x26;
    
    if (final_p == expected_p) {
        std::cout << "✅ SUCCESS: Test passes (ProcessorTests issue might be elsewhere)" << std::endl;
        return 0;
    } else {
        std::cout << "❌ FAILURE: Test reproduces ProcessorTests failure" << std::endl;
        std::cout << "Difference: 0x" << std::hex << (int)(final_p ^ expected_p) << std::dec << std::endl;
        
        // Analyze the difference
        if ((final_p ^ expected_p) == 0x40) {
            std::cout << "CONFIRMED: V flag incorrectly set (bit 6)" << std::endl;
        } else {
            std::cout << "Different failure pattern than ProcessorTests" << std::endl;
        }
        return 1;
    }
}