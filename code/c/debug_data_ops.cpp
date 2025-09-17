#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

int main() {
    using namespace fam65xx_cpp;
    
    // Create 6502 CPU and reset
    fam65xx_with_cycle_count<config_6502> cpu;
    cpu.init_for_test();
    
    // Memory array for testing
    uint8_t memory[0x10000];
    std::fill(memory, memory + sizeof(memory), 0xEA); // Fill with NOP
    
    // Setup ASL $40 instruction at $0000
    memory[0x0000] = 0x06;  // ASL $40 opcode
    memory[0x0001] = 0x40;  // Zero page address
    memory[0x0040] = 0x40;  // Memory value to shift
    
    // Set PC to instruction
    cpu.set_pc(0x0000);
    
    std::cout << "=== DEBUGGING DATA OPERATIONS FOR ASL $40 ===" << std::endl;
    std::cout << "Initial state:" << std::endl;
    std::cout << "PC: $" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "Memory[$40]: $" << std::hex << std::setfill('0') << std::setw(2) << (int)memory[0x40] << std::endl;
    std::cout << std::endl;
    
    // Execute cycles and monitor cycle descriptions
    bus_state_t bus_state = 0;
    int cycle_count = 0;
    
    do {
        cycle_count++;
        std::cout << "--- Cycle " << cycle_count << " ---" << std::endl;
        
        uint16_t addr = cpu.get_address();
        uint8_t data = memory[addr];
        bool is_write = !cpu.get_rw();
        
        std::cout << "Address: $" << std::hex << std::setfill('0') << std::setw(4) << addr << std::endl;
        std::cout << "R/W: " << (is_write ? "WRITE" : "READ") << std::endl;
        
        // Get cycle description to see DataOp values
        if (cpu.get_cycle_step() > 0) {
            uint16_t opcode = cpu.get_opcode();
            uint8_t cycle_step = cpu.get_cycle_step();
            
            std::cout << "Opcode: 0x" << std::hex << std::setfill('0') << std::setw(2) << opcode << std::endl;
            std::cout << "Cycle Step: " << std::dec << (int)cycle_step << std::endl;
            
            // Print the values we can access - for now just show we're in the right cycle
            std::cout << "This is write cycle " << cycle_count << " for memory modify operation" << std::endl;
        }
        
        if (is_write) {
            // Get write data BEFORE cycle_tick() (as test harness does)
            uint8_t write_data = cpu.get_write_data();
            std::cout << "get_write_data() BEFORE cycle_tick(): $" << std::hex << std::setfill('0') << std::setw(2) << (int)write_data << std::endl;
            
            // Write to memory
            memory[addr] = write_data;
            std::cout << "WRITING $" << std::hex << std::setfill('0') << std::setw(2) << (int)write_data << " to address $" << std::setw(4) << addr << std::endl;
        } else {
            std::cout << "Reading $" << std::hex << std::setfill('0') << std::setw(2) << (int)data << " from address $" << std::setw(4) << addr << std::endl;
        }
        
        // Execute the cycle
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "PC after cycle: $" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
        std::cout << "Cycle step after: " << std::dec << (int)cpu.get_cycle_step() << std::endl;
        std::cout << std::endl;
        
    } while (cycle_count < 6 && cpu.get_cycle_step() != 0);
    
    std::cout << "Final memory[$40]: $" << std::hex << std::setfill('0') << std::setw(2) << (int)memory[0x40] << std::endl;
    
    return 0;
}