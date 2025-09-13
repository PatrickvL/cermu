#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>

// Simple memory implementation for debugging
class DebugMemory {
private:
    uint8_t memory[65536];
    
public:
    DebugMemory() {
        // Clear memory
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
        
        // Set reset vector to match Klaus test binary
        memory[0xFFFC] = 0x9d;  // Low byte of $379d
        memory[0xFFFD] = 0x37;  // High byte of $379d
        
        // Put a simple infinite loop at $0400 for testing
        memory[0x0400] = 0x4C;  // JMP absolute
        memory[0x0401] = 0x00;  // Jump to $0400 (infinite loop)
        memory[0x0402] = 0x04;
    }
    
    uint8_t read(uint16_t addr) {
        std::cout << "Memory read: $" << std::hex << std::setw(4) << std::setfill('0') 
                  << addr << " = $" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)memory[addr] << std::dec << std::endl;
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        std::cout << "Memory write: $" << std::hex << std::setw(4) << std::setfill('0') 
                  << addr << " = $" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)data << std::dec << std::endl;
        memory[addr] = data;
    }
};

int main() {
    std::cout << "=== DEBUG RESET SEQUENCE TEST ===" << std::endl;
    
    // Create CPU and memory
    fam65xx_cpp::fam65xx<config_6502> cpu;
    DebugMemory memory;
    
    std::cout << "Initial CPU state:" << std::endl;
    std::cout << "PC: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::dec << std::endl;
    std::cout << "State flags: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_state_flags() << std::dec << std::endl;
    std::cout << "Opcode: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_opcode() << std::dec << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Execute cycles and trace the reset sequence
    for (int cycle = 0; cycle < 20; cycle++) {
        std::cout << "=== CYCLE " << cycle + 1 << " ===" << std::endl;
        
        // Set up bus state
        bus_state_t bus_state = 0;
        bus_state |= BUS_BIT(BUS_RW_BIT);  // Default to read
        bus_state |= BUS_BIT(BUS_RDY_BIT); // CPU is ready
        
        // Execute CPU cycle
        std::cout << "Before cycle_tick:" << std::endl;
        std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::dec << std::endl;
        std::cout << "  State flags: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_state_flags() << std::dec << std::endl;
        std::cout << "  Opcode: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_opcode() << std::dec << std::endl;
        std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
        
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "After cycle_tick:" << std::endl;
        std::cout << "  Address: $" << std::hex << std::setw(4) << std::setfill('0') << BUS_GET_ADDR(bus_state) << std::dec << std::endl;
        std::cout << "  RW: " << ((bus_state & BUS_BIT(BUS_RW_BIT)) ? "READ" : "write") << std::endl;
        
        // Handle memory operations
        if (bus_state & BUS_BIT(BUS_RW_BIT)) {
            // Read cycle
            uint16_t addr = BUS_GET_ADDR(bus_state);
            uint8_t data = memory.read(addr);
            BUS_SET_DATA(bus_state, data);
            std::cout << "  Data on bus: $" << std::hex << std::setw(2) << std::setfill('0') << (int)data << std::dec << std::endl;
        } else {
            // Write cycle
            uint16_t addr = BUS_GET_ADDR(bus_state);
            uint8_t data = BUS_GET_DATA(bus_state);
            memory.write(addr, data);
            std::cout << "  Data written: $" << std::hex << std::setw(2) << std::setfill('0') << (int)data << std::dec << std::endl;
        }
        
        std::cout << "Final state:" << std::endl;
        std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::dec << std::endl;
        std::cout << "  State flags: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_state_flags() << std::dec << std::endl;
        std::cout << "  Opcode: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_opcode() << std::dec << std::endl;
        std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
        std::cout << std::endl;
        
        // Check if PC has changed from $0000
        if (cpu.get_pc() != 0x0000) {
            std::cout << "SUCCESS: PC changed to $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::dec << std::endl;
            break;
        }
        
        // Stop if we've clearly finished reset sequence
        if (cycle > 10 && cpu.get_cycle_step() == 0) {
            std::cout << "Reset sequence appears complete but PC still at $0000" << std::endl;
            break;
        }
    }
    
    return 0;
}