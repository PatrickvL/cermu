#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

int main() {
    fam65xx<config_6502> cpu;
    cpu.init();
    cpu.clear_state(STATE_RESET_PENDING);
    
    // Set up simple state
    cpu.set_pc(0x0200);
    cpu.set_sp(0xFF);
    cpu.set_status(0x24);
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: $" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  State flags: $" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "  Opcode: $" << std::hex << cpu.get_opcode() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Simulate memory with BRK at $0200
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0x00); // BRK opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    
    std::cout << "\nExecuting cycle 1 (opcode fetch):" << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 1:" << std::endl;
    std::cout << "  PC: $" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  State flags: $" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "  Opcode: $" << std::hex << cpu.get_opcode() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << "  In interrupt sequence: " << ((cpu.get_state_flags() & STATE_INTERRUPT_SEQUENCE) ? "YES" : "NO") << std::endl;
    std::cout << "  Next address: $" << std::hex << cpu.get_address() << std::endl;
    
    // Debug the cycle table lookup
    std::cout << "  Checking cycle table for opcode $" << std::hex << cpu.get_opcode()
              << ", step " << (int)cpu.get_cycle_step() << std::endl;
    
    // Execute cycle 2 (first interrupt sequence cycle)
    std::cout << "\nExecuting cycle 2 (interrupt sequence):" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, 0x42); // Dummy data
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 2:" << std::endl;
    std::cout << "  PC: $" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << "  Next address: $" << std::hex << cpu.get_address() << std::endl;
    
    // Execute cycle 3 (stack push PCH)
    std::cout << "\nExecuting cycle 3 (stack push PCH):" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, 0x00); // Dummy data
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 3:" << std::endl;
    std::cout << "  PC: $" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << "  Next address: $" << std::hex << cpu.get_address() << std::endl;
    
    return 0;
}