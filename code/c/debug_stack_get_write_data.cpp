#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

using namespace fam65xx_cpp;

// Mock memory system
uint8_t memory[65536];

uint8_t read_memory(uint16_t addr) {
    return memory[addr];
}

void write_memory(uint16_t addr, uint8_t data) {
    std::cout << "    DEBUG: Writing 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)data 
              << " to address 0x" << std::hex << std::setw(4) << std::setfill('0') << addr << "\n";
    memory[addr] = data;
}

int main() {
    std::cout << "=== DEBUGGING GET_WRITE_DATA() FOR PHA ===\n";
    
    // Initialize CPU for testing
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_a(0x42);     // A = 0x42 (value to push)
    cpu.set_p(0xc3);     // P = 0xc3
    cpu.set_s(0xff);     // S = 0xff (stack starts full)
    cpu.set_pc(0x1000);  // PC = 0x1000
    
    // Set up memory with PHA instruction
    memory[0x1000] = 0x48;  // PHA opcode
    memory[0x1001] = 0xea;  // NOP (dummy)
    
    std::cout << "Initial state:\n";
    std::cout << "A: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << "\n";
    std::cout << "S: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_s() << "\n";
    std::cout << "PC: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << "\n";
    std::cout << "\n";
    
    // Execute PHA instruction cycle by cycle
    for (int cycle = 0; cycle < 3; cycle++) {  // PHA takes 3 cycles
        std::cout << "--- Cycle " << (cycle + 1) << " ---\n";
        
        // Get address and R/W state BEFORE execution
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        std::cout << "Address: $" << std::hex << std::setw(4) << std::setfill('0') << addr << "\n";
        std::cout << "R/W: " << (is_read ? "READ" : "WRITE") << "\n";
        
        if (!is_read) {
            // This is a write cycle - get the data that should be written
            uint8_t write_data = cpu.get_write_data();
            std::cout << "get_write_data() BEFORE cycle_tick(): $" << std::hex << std::setw(2) << std::setfill('0') << (int)write_data << "\n";
        }
        
        // Create bus state
        bus_state_t bus_state = 0;
        BUS_SET_ADDR(bus_state, addr);
        if (is_read) {
            bus_state |= BUS_RW_BIT;  // Set R/W high for read
        } else {
            bus_state &= ~BUS_RW_BIT; // Set R/W low for write
        }
        
        if (is_read) {
            // Read cycle
            uint8_t data = read_memory(addr);
            BUS_SET_DATA(bus_state, data);
            std::cout << "Reading $" << std::hex << std::setw(2) << std::setfill('0') << (int)data
                      << " from address $" << std::hex << std::setw(4) << std::setfill('0') << addr << "\n";
        } else {
            // Write cycle
            uint8_t write_data = cpu.get_write_data();
            BUS_SET_DATA(bus_state, write_data);
            write_memory(addr, write_data);
        }
        
        // Execute the cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "PC after cycle: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << "\n";
        std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << "\n";
        std::cout << "\n";
        
        // If instruction is complete, break
        if (cpu.get_cycle_step() == 0) {
            break;
        }
    }
    
    std::cout << "Final stack[0x01FF]: $" << std::hex << std::setw(2) << std::setfill('0') << (int)memory[0x01FF] << "\n";
    std::cout << "Final S: $" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_s() << "\n";
    
    return 0;
}