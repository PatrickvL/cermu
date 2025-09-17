#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

// Simple test memory class
class TestMemory {
public:
    uint8_t memory[0x10000];
    
    TestMemory() {
        // Clear memory
        for (int i = 0; i < 0x10000; i++) {
            memory[i] = 0;
        }
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    using Config = config_6502;
    fam65xx<Config> cpu;
    TestMemory mem;
    
    std::cout << "=== Branch Conditional Cycle Debug ===" << std::endl;
    std::cout << "Testing branch instruction timing with new conditional cycle logic." << std::endl;
    
    // Test case 1: BPL with N=0 (should branch, 3 cycles expected)
    std::cout << "\n--- Test Case 1: BPL with N=0 (should branch) ---" << std::endl;
    
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    cpu.set_p(0x00); // N=0, should branch
    
    // Set up memory: BPL +5
    mem.write(0x1000, 0x10); // BPL opcode
    mem.write(0x1001, 0x05); // Offset +5
    
    bus_state_t bus_state = 0;
    int cycle_count = 0;
    
    std::cout << "Initial: PC=0x" << std::hex << cpu.get_pc() << " P=0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Execute instruction cycle by cycle
    while (cpu.get_cycle_step() != 0 || cycle_count == 0) {
        uint16_t addr = cpu.get_address();
        uint8_t data = mem.read(addr);
        bool rw = cpu.get_rw();
        
        bus_state = BUS_SET_DATA(bus_state, data);
        
        std::cout << "Cycle " << cycle_count << ": "
                  << "addr=0x" << std::hex << addr 
                  << " data=0x" << std::hex << (int)data
                  << " rw=" << (rw ? "R" : "W")
                  << " step=" << (int)cpu.get_cycle_step()
                  << " flags=0x" << std::hex << cpu.get_state_flags()
                  << std::endl;
        
        bus_state = cpu.cycle_tick(bus_state);
        cycle_count++;
        
        if (cycle_count > 10) break; // Safety
    }
    
    std::cout << "Final: PC=0x" << std::hex << cpu.get_pc() 
              << " Cycles=" << cycle_count 
              << " Expected: PC=0x1007, 3 cycles" << std::endl;
    
    // Test case 2: BPL with N=1 (should not branch, 2 cycles expected)
    std::cout << "\n--- Test Case 2: BPL with N=1 (should not branch) ---" << std::endl;
    
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    cpu.set_p(0x80); // N=1, should not branch
    
    bus_state = 0;
    cycle_count = 0;
    
    std::cout << "Initial: PC=0x" << std::hex << cpu.get_pc() << " P=0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Execute instruction cycle by cycle
    while (cpu.get_cycle_step() != 0 || cycle_count == 0) {
        uint16_t addr = cpu.get_address();
        uint8_t data = mem.read(addr);
        bool rw = cpu.get_rw();
        
        bus_state = BUS_SET_DATA(bus_state, data);
        
        std::cout << "Cycle " << cycle_count << ": "
                  << "addr=0x" << std::hex << addr 
                  << " data=0x" << std::hex << (int)data
                  << " rw=" << (rw ? "R" : "W")
                  << " step=" << (int)cpu.get_cycle_step()
                  << " flags=0x" << std::hex << cpu.get_state_flags()
                  << std::endl;
        
        bus_state = cpu.cycle_tick(bus_state);
        cycle_count++;
        
        if (cycle_count > 10) break; // Safety
    }
    
    std::cout << "Final: PC=0x" << std::hex << cpu.get_pc() 
              << " Cycles=" << cycle_count 
              << " Expected: PC=0x1002, 2 cycles" << std::endl;
    
    return 0;
}