#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== PRECISE PHA INSTRUCTION DEBUGGING ===" << std::endl;
    
    // Create a 6502 CPU instance
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state exactly like ProcessorTests
    cpu.set_pc(0x5063);
    cpu.set_a(0x3e);
    cpu.set_s(0x9f);
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "  A: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_a() << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_s() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Set up bus with Ready line high and initial data
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Ready line high
    bus_state = BUS_SET_DATA(bus_state, 0x48);  // PHA opcode
    
    std::cout << "\nExecuting opcode fetch (cycle 0):" << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After opcode fetch:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "  Opcode: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_opcode() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << "  Address: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_address() << std::endl;
    std::cout << "  RW: " << (cpu.get_rw() ? "READ" : "write") << std::endl;
    
    // Execute cycle 1 (dummy read)
    std::cout << "\nExecuting cycle 1 (dummy read):" << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 1:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << "  Address: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_address() << std::endl;
    std::cout << "  RW: " << (cpu.get_rw() ? "read" : "write") << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_s() << std::endl;
    
    if (!cpu.get_rw()) {
        std::cout << "  Write data: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_write_data() << std::endl;
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "  A: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_a() << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_s() << std::endl;
    std::cout << "  Stack address (0x0100 + SP): 0x" << std::hex << std::setfill('0') << std::setw(4) << (0x0100 + cpu.get_s()) << std::endl;
    
    return 0;
}