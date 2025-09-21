// Debug ASL A instruction cycle by cycle to see what's happening
#include <iostream>
#include <iomanip>
#include <vector>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

int main() {
    auto cpu = fam65xx<config_6502>();
    cpu.init_for_test();
    
    // Test case: A=0x42, P=0x00, PC=0x0000
    cpu.set_pc(0x0000);
    cpu.set_a(0x42);
    cpu.set_p(0x00);
    
    std::cout << "=== ASL A Instruction Cycle-by-Cycle Debug ===" << std::endl;
    std::cout << "Initial state: PC=0x" << std::hex << cpu.get_pc() 
              << " A=0x" << (int)cpu.get_a() << " P=0x" << (int)cpu.get_p() << std::endl;
    
    // Create a simple memory simulation
    std::vector<uint8_t> memory(65536, 0);
    memory[0x0000] = 0x0A; // ASL A instruction
    
    // Execute instruction cycle by cycle
    for (int cycle = 0; cycle < 3; cycle++) {
        std::cout << "\n--- Cycle " << cycle << " ---" << std::endl;
        
        // Get CPU state before cycle
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        std::cout << "Before tick: addr=0x" << std::hex << addr
                  << " rw=" << (is_read ? "R" : "W") << std::endl;
        
        uint8_t data = 0;
        if (is_read) {
            data = memory[addr];
            std::cout << "Reading data=0x" << std::hex << (int)data << " from addr=0x" << addr << std::endl;
        } else {
            data = cpu.get_write_data();
            memory[addr] = data;
            std::cout << "Writing data=0x" << std::hex << (int)data << " to addr=0x" << addr << std::endl;
        }
        
        // Create bus state with data
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state |= BUS_BIT(BUS_RDY_BIT); // RDY high (ready)
        
        cpu.cycle_tick(bus_state);
        
        // Get CPU state after cycle
        std::cout << "After tick:  PC=0x" << std::hex << cpu.get_pc()
                  << " A=0x" << (int)cpu.get_a() << " P=0x" << (int)cpu.get_p() << std::endl;
    }
    
    std::cout << "\n=== Final Results ===" << std::endl;
    std::cout << "Expected: PC=0x0001 A=0x84 P=0x80" << std::endl;
    std::cout << "Got:      PC=0x" << std::hex << cpu.get_pc() 
              << " A=0x" << (int)cpu.get_a() 
              << " P=0x" << (int)cpu.get_p() << std::endl;
    
    bool pc_match = (cpu.get_pc() == 0x0001);
    bool a_match = (cpu.get_a() == 0x84);
    bool p_match = (cpu.get_p() == 0x80);
    
    std::cout << "PC match: " << (pc_match ? "YES" : "NO") << std::endl;
    std::cout << "A match:  " << (a_match ? "YES" : "NO") << std::endl;
    std::cout << "P match:  " << (p_match ? "YES" : "NO") << std::endl;
    
    if (pc_match && a_match && p_match) {
        std::cout << "✅ ASL A instruction working correctly!" << std::endl;
    } else {
        std::cout << "❌ ASL A instruction has issues" << std::endl;
    }
    
    return 0;
}