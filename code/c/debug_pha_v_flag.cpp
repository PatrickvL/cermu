#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

int main() {
    std::cout << "=== PHA V Flag Debug Test ===" << std::endl;
    
    // Create CPU
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up test state similar to ProcessorTests
    cpu.set_pc(0x5063);
    cpu.set_a(0x3e);
    cpu.set_sp(0xa0);
    cpu.set_status(0xac);  // Initial P register value (V=0)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "  V flag: " << ((cpu.get_status() & 0x40) ? 1 : 0) << std::endl;
    
    // Prepare bus state with PHA opcode
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0x48);  // PHA opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);  // SO pin inactive (high)
    
    std::cout << "\nExecuting opcode fetch (cycle 0)..." << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After opcode fetch:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "  V flag: " << ((cpu.get_status() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Execute cycle 1 (dummy read)
    std::cout << "\nExecuting cycle 1 (dummy read)..." << std::endl;
    uint16_t addr = cpu.get_address();
    std::cout << "  Address: 0x" << std::hex << addr << std::endl;
    
    bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0x00);  // Dummy data
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);  // SO pin inactive (high)
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 1:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "  V flag: " << ((cpu.get_status() & 0x40) ? 1 : 0) << " <-- CHECK FOR V FLAG CHANGE" << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Execute cycle 2 (stack write)
    std::cout << "\nExecuting cycle 2 (stack write)..." << std::endl;
    addr = cpu.get_address();
    bool is_write = !cpu.get_rw();
    uint8_t write_data = cpu.get_write_data();
    
    std::cout << "  Address: 0x" << std::hex << addr << std::endl;
    std::cout << "  Is write: " << (is_write ? "YES" : "NO") << std::endl;
    std::cout << "  Write data: 0x" << std::hex << (int)write_data << std::endl;
    
    bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, write_data);
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);  // SO pin inactive (high)
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 2 (FINAL):" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "  V flag: " << ((cpu.get_status() & 0x40) ? 1 : 0) << " <-- FINAL V FLAG STATE" << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  PC: 0x5064" << std::endl;
    std::cout << "  P: 0xac (V=0)" << std::endl;
    std::cout << "  Stack[0x19f]: 0x3e" << std::endl;
    
    return 0;
}