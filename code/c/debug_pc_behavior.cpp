#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

// Mock bus implementation for debugging
class TestBus {
private:
    std::map<uint16_t, uint8_t> memory;
    
public:
    TestBus() {
        // Set up test memory with PHA at 0x1000, NOP at 0x1001
        memory[0x1000] = 0x48; // PHA
        memory[0x1001] = 0xEA; // NOP 
        memory[0x1002] = 0x00; // BRK (to stop execution)
    }
    
    uint8_t read(uint16_t address) {
        auto it = memory.find(address);
        return (it != memory.end()) ? it->second : 0x00;
    }
    
    void write(uint16_t address, uint8_t data) {
        memory[address] = data;
    }
};

int main() {
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    TestBus bus;
    
    std::cout << "=== PC Behavior Debug ===" << std::endl;
    
    // Initialize CPU for testing
    cpu.init_for_test();
    cpu.set_pc(0x1000);  // Point to PHA instruction
    cpu.set_a(0x42);     // Set accumulator to 0x42
    cpu.set_s(0xFF);     // Stack pointer at top
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "Opcode at 0x1000: 0x" << std::hex << (int)bus.read(0x1000) << std::endl;
    std::cout << "Opcode at 0x1001: 0x" << std::hex << (int)bus.read(0x1001) << std::endl;
    std::cout << std::endl;
    
    // Execute several cycles and track PC changes
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t pc_before = cpu.get_pc();
        uint8_t step_before = cpu.get_cycle_step();
        uint16_t opcode_before = cpu.get_opcode();
        
        // Get address that will be accessed
        uint16_t address = cpu.get_address();
        uint8_t data = bus.read(address);
        bool is_write = !cpu.get_rw();
        
        // Set up bus state
        bus_state_t bus_state = 0;
        BUS_SET_ADDR(bus_state, address);
        BUS_SET_DATA(bus_state, data);
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY = ready
        bus_state |= BUS_BIT(BUS_RW_BIT);   // R/W = read (default)
        
        // Execute one cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        uint16_t pc_after = cpu.get_pc();
        uint8_t step_after = cpu.get_cycle_step();
        uint16_t opcode_after = cpu.get_opcode();
        
        std::cout << "Cycle " << std::dec << cycle << ":" << std::endl;
        std::cout << "  Before: PC=0x" << std::hex << pc_before 
                  << ", step=" << std::dec << (int)step_before 
                  << ", opcode=0x" << std::hex << opcode_before << std::endl;
        std::cout << "  Access: addr=0x" << std::hex << address 
                  << ", data=0x" << std::hex << (int)data 
                  << ", " << (is_write ? "WRITE" : "READ") << std::endl;
        std::cout << "  After:  PC=0x" << std::hex << pc_after 
                  << ", step=" << std::dec << (int)step_after 
                  << ", opcode=0x" << std::hex << opcode_after << std::endl;
        
        // Check if PC changed
        if (pc_after != pc_before) {
            std::cout << "  *** PC CHANGED: 0x" << std::hex << pc_before 
                      << " -> 0x" << pc_after << " (+" << std::dec << (pc_after - pc_before) << ")" << std::endl;
        }
        
        std::cout << std::endl;
        
        // Stop if we've completed the instruction
        if (step_after == 0 && cycle > 0) {
            std::cout << "Instruction completed after cycle " << cycle << std::endl;
            std::cout << "Next instruction should be fetched from PC=0x" << std::hex << cpu.get_pc() << std::endl;
            
            // Continue for one more cycle to see what happens
            if (cycle < 5) {
                std::cout << "Continuing to see next instruction fetch..." << std::endl;
                continue;
            }
            break;
        }
    }
    
    return 0;
}