#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>

int main() {
    using Config = config_6502;
    using CPU = fam65xx_cpp::fam65xx<Config>;
    
    CPU cpu;
    cpu.init_for_test();
    
    // Set up a simple LDA #$cc instruction
    // PC starts at 0x0000, instruction at 0x0000 = 0xa9, immediate at 0x0001 = 0xcc
    cpu.set_pc(0x0000);
    
    std::cout << "=== Debugging LDA #$cc instruction ===" << std::endl;
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    
    // Cycle 0: Fetch opcode (0xa9)
    std::cout << "\n--- Cycle 0: Opcode fetch ---" << std::endl;
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0xa9);  // LDA immediate opcode
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After opcode fetch:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  Opcode: 0x" << std::hex << cpu.get_opcode() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Cycle 1: Read immediate byte (0xcc)
    std::cout << "\n--- Cycle 1: Immediate byte read ---" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, 0xcc);  // Immediate value
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After immediate read:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    if (cpu.get_a() == 0xcc) {
        std::cout << "\n✓ SUCCESS: LDA instruction worked correctly!" << std::endl;
    } else {
        std::cout << "\n✗ FAILURE: A register should be 0xcc but is 0x" 
                  << std::hex << (int)cpu.get_a() << std::endl;
    }
    
    return 0;
}