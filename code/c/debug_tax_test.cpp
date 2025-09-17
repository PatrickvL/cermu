#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== TAX Instruction Debug Test ===" << std::endl;
    
    // Create CPU with 6502 configuration
    fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];
    
    // Initialize memory
    std::fill(memory, memory + 65536, 0);
    
    // Use test initialization (no reset pending)
    cpu.init_for_test();
    
    // Set test values
    cpu.set_a(0x42);      // Set accumulator to test value
    cpu.set_x(0x00);      // Clear X register
    cpu.set_status(0x00); // Clear status flags
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "A = 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "X = 0x" << std::hex << (int)cpu.get_x() << std::endl;
    std::cout << "P = 0x" << std::hex << (int)cpu.get_status() << std::endl;
    
    // Set up memory with TAX instruction at address 0x0000
    // TAX opcode is 0xAA
    memory[0x0000] = 0xAA;
    memory[0x0001] = 0xEA; // NOP for next instruction
    
    // Set PC to start of program
    cpu.set_pc(0x0000);
    
    std::cout << "\nExecuting TAX instruction..." << std::endl;
    
    // Execute one instruction (TAX should complete in 2 cycles)
    for (int cycle = 0; cycle < 3; cycle++) {
        std::cout << "Cycle " << cycle << ":" << std::endl;
        std::cout << "  PC = 0x" << std::hex << std::setfill('0') << std::setw(4)
                  << cpu.get_pc() << std::endl;
        std::cout << "  A = 0x" << std::hex << (int)cpu.get_a() << std::endl;
        std::cout << "  X = 0x" << std::hex << (int)cpu.get_x() << std::endl;
        std::cout << "  P = 0x" << std::hex << (int)cpu.get_status() << std::endl;
        std::cout << "  Step = " << (int)cpu.get_cycle_step() << std::endl;
        
        if (cycle < 2) {
            // Create bus state for cycle execution
            uint16_t addr = cpu.get_address();
            bool is_write = !cpu.get_rw();
            
            bus_state_t bus_state = 0;
            if (is_write) {
                // Write cycle
                uint8_t data = cpu.get_write_data();
                memory[addr] = data;
                BUS_SET_DATA(bus_state, data);
                std::cout << "  Writing 0x" << std::hex << (int)data << " to 0x" << addr << std::endl;
            } else {
                // Read cycle
                uint8_t data = memory[addr];
                BUS_SET_DATA(bus_state, data);
                std::cout << "  Reading 0x" << std::hex << (int)data << " from 0x" << addr << std::endl;
            }
            
            // Set RDY line (ready)
            bus_state |= BUS_BIT(BUS_RDY_BIT);
            
            // Execute one CPU cycle
            bus_state = cpu.cycle_tick(bus_state);
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "A = 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "X = 0x" << std::hex << (int)cpu.get_x() << std::endl;
    std::cout << "P = 0x" << std::hex << (int)cpu.get_status() << std::endl;
    
    // Check expected result
    if (cpu.get_x() == 0x42 && (cpu.get_status() & P_NEGATIVE) == 0 && (cpu.get_status() & P_ZERO) == 0) {
        std::cout << "\n✅ TAX instruction executed correctly!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ TAX instruction failed!" << std::endl;
        std::cout << "Expected: X=0x42, N=0, Z=0" << std::endl;
        std::cout << "Got: X=0x" << std::hex << (int)cpu.get_x()
                  << ", N=" << ((cpu.get_status() & P_NEGATIVE) ? 1 : 0)
                  << ", Z=" << ((cpu.get_status() & P_ZERO) ? 1 : 0) << std::endl;
        return 1;
    }
}