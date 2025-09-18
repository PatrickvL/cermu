#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

int main() {
    std::cout << "=== SO Pin State Debug Test ===" << std::endl;
    
    // Create CPU
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up test state
    cpu.set_pc(0x5063);
    cpu.set_a(0x3e);
    cpu.set_sp(0xa0);
    cpu.set_status(0xac);  // Initial P register value (V=0)
    
    std::cout << "Testing SO pin state detection..." << std::endl;
    
    // Test 1: SO pin HIGH (inactive) - should NOT set V flag
    std::cout << "\nTest 1: SO pin HIGH (inactive)" << std::endl;
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0x48);  // PHA opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT);         // RDY high (ready)
    bus_state |= BUS_BIT(BUS_SO_BIT);          // SO pin HIGH (inactive)
    
    std::cout << "  Bus state: 0x" << std::hex << bus_state << std::endl;
    std::cout << "  SO bit set: " << ((bus_state & BUS_BIT(BUS_SO_BIT)) ? "YES" : "NO") << std::endl;
    std::cout << "  SO pin state: " << ((bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH (inactive)" : "LOW (active)") << std::endl;
    
    uint8_t initial_p = cpu.get_status();
    std::cout << "  Initial P: 0x" << std::hex << (int)initial_p << std::endl;
    std::cout << "  Initial V flag: " << ((initial_p & 0x40) ? 1 : 0) << std::endl;
    
    // Execute one cycle
    bus_state = cpu.cycle_tick(bus_state);
    
    uint8_t final_p = cpu.get_status();
    std::cout << "  Final P: 0x" << std::hex << (int)final_p << std::endl;
    std::cout << "  Final V flag: " << ((final_p & 0x40) ? 1 : 0) << std::endl;
    std::cout << "  V flag changed: " << ((initial_p & 0x40) != (final_p & 0x40) ? "YES" : "NO") << std::endl;
    
    // Test 2: SO pin LOW (active) - should set V flag
    std::cout << "\nTest 2: SO pin LOW (active)" << std::endl;
    cpu.set_status(0xac);  // Reset P register (V=0)
    
    bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0xea);  // NOP opcode (not PHA)
    bus_state |= BUS_BIT(BUS_RDY_BIT);         // RDY high (ready)
    // SO pin LOW (active) - do not set BUS_SO_BIT
    
    std::cout << "  Bus state: 0x" << std::hex << bus_state << std::endl;
    std::cout << "  SO bit set: " << ((bus_state & BUS_BIT(BUS_SO_BIT)) ? "YES" : "NO") << std::endl;
    std::cout << "  SO pin state: " << ((bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH (inactive)" : "LOW (active)") << std::endl;
    
    initial_p = cpu.get_status();
    std::cout << "  Initial P: 0x" << std::hex << (int)initial_p << std::endl;
    std::cout << "  Initial V flag: " << ((initial_p & 0x40) ? 1 : 0) << std::endl;
    
    // Execute one cycle
    bus_state = cpu.cycle_tick(bus_state);
    
    final_p = cpu.get_status();
    std::cout << "  Final P: 0x" << std::hex << (int)final_p << std::endl;
    std::cout << "  Final V flag: " << ((final_p & 0x40) ? 1 : 0) << std::endl;
    std::cout << "  V flag changed: " << ((initial_p & 0x40) != (final_p & 0x40) ? "YES" : "NO") << std::endl;
    
    return 0;
}