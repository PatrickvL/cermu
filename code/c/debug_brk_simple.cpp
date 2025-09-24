#include <iostream>
#include <cstdint>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
using namespace fam65xx_cpp;

int main() {
    // Create CPU with 6502 configuration  
    auto cpu = fam65xx<cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, 16, false, false, false>>();
    cpu.init_for_test();
    
    // Set up initial state from failing test case
    cpu.set_pc(0x8b82);
    cpu.set_a(0xcb);
    cpu.set_x(0x75);
    cpu.set_y(0xa2);
    cpu.set_sp(0x51);
    cpu.set_p(0x6a);
    
    std::cout << "=== Before BRK ===" << std::endl;
    std::cout << "PC: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_a() << std::endl;
    std::cout << "X: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_x() << std::endl;
    std::cout << "Y: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_y() << std::endl;
    std::cout << "SP: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_sp() << std::endl;
    std::cout << "P: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_p() << std::endl;
    
    // Execute BRK instruction (0x00) for 7 cycles
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0x00);  // BRK opcode on bus
    for (int cycle = 0; cycle < 7; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;
        bus_state = cpu.cycle_tick(bus_state);
        std::cout << "After cycle " << cycle << ": P = 0x" << std::hex << (int)cpu.get_p() << std::endl;
    }
    
    std::cout << "\n=== After BRK ===" << std::endl;
    std::cout << "PC: 0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_a() << std::endl;
    std::cout << "X: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_x() << std::endl;
    std::cout << "Y: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_y() << std::endl;
    std::cout << "SP: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_sp() << std::endl;
    std::cout << "P: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_p() << std::endl;
    
    std::cout << "\nExpected P: 0x6e" << std::endl;
    std::cout << "Actual P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "Difference: 0x" << std::hex << ((int)cpu.get_p() ^ 0x6e) << std::endl;
    
    return 0;
}
