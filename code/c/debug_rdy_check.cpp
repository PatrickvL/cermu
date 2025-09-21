#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== RDY Line Debug ===" << std::endl;
    
    // Create CPU instance
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_pc(0x1000);
    cpu.set_a(0x42);
    cpu.set_s(0xFF);
    
    // Memory simulation: PHA at 0x1000
    uint8_t memory[65536];
    memory[0x1000] = 0x48;  // PHA
    memory[0x1001] = 0xEA;  // NOP (next instruction)
    
    bus_state_t bus_state = 0;
    
    std::cout << "\n--- Checking RDY Line State ---" << std::endl;
    
    // Check initial bus state
    std::cout << "Initial bus_state: 0x" << std::hex << bus_state << std::endl;
    std::cout << "RDY bit set: " << ((bus_state & BUS_BIT(BUS_RDY_BIT)) ? "YES" : "NO") << std::endl;
    std::cout << "RDY bit value: " << BUS_BIT(BUS_RDY_BIT) << std::endl;
    
    // Set RDY line high (ready)
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    std::cout << "After setting RDY high: 0x" << std::hex << bus_state << std::endl;
    std::cout << "RDY bit now set: " << ((bus_state & BUS_BIT(BUS_RDY_BIT)) ? "YES" : "NO") << std::endl;
    
    std::cout << "\n--- Cycle 0: Opcode Fetch ---" << std::endl;
    uint16_t addr = cpu.get_address();
    uint8_t bus_data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, bus_data);
    std::cout << "Before: Step=" << (int)cpu.get_cycle_step() << ", RDY=" << ((bus_state & BUS_BIT(BUS_RDY_BIT)) ? "HIGH" : "LOW") << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After: Step=" << (int)cpu.get_cycle_step() << ", RDY=" << ((bus_state & BUS_BIT(BUS_RDY_BIT)) ? "HIGH" : "LOW") << std::endl;
    
    std::cout << "\n--- Cycle 1: Should advance ---" << std::endl;
    addr = cpu.get_address();
    bus_data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, bus_data);
    std::cout << "Before: Step=" << (int)cpu.get_cycle_step() << ", RDY=" << ((bus_state & BUS_BIT(BUS_RDY_BIT)) ? "HIGH" : "LOW") << std::endl;
    
    // Check CPU state flags
    std::cout << "CPU state flags: 0x" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "STATE_RDY_WAIT set: " << (cpu.get_state_flags() & STATE_RDY_WAIT ? "YES" : "NO") << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After: Step=" << (int)cpu.get_cycle_step() << ", RDY=" << ((bus_state & BUS_BIT(BUS_RDY_BIT)) ? "HIGH" : "LOW") << std::endl;
    std::cout << "CPU state flags after: 0x" << std::hex << cpu.get_state_flags() << std::endl;
    
    return 0;
}