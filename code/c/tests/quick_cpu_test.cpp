#include <iostream>
#include <iomanip>
#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include "../src/core/system_lines.h"

// Simple memory implementation for testing
class SimpleTestMemory {
private:
    uint8_t memory[65536];
    
public:
    SimpleTestMemory() {
        // Initialize memory to zero
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
        
        // Set up reset vector to point to $1000
        memory[0xFFFC] = 0x00;  // Reset vector low byte
        memory[0xFFFD] = 0x10;  // Reset vector high byte ($1000)
        
        // Simple test program at $1000:
        // LDA #$42    ; Load $42 into A
        // NOP         ; No operation
        // JMP $1000   ; Jump back to start (infinite loop)
        memory[0x1000] = 0xA9;  // LDA immediate
        memory[0x1001] = 0x42;  // Load value $42
        memory[0x1002] = 0xEA;  // NOP
        memory[0x1003] = 0x4C;  // JMP absolute
        memory[0x1004] = 0x00;  // Jump target low byte
        memory[0x1005] = 0x10;  // Jump target high byte ($1000)
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    std::cout << "========================================\n";
    std::cout << "Quick CPU Test - Interrupt Vector Optimization Validation\n";
    std::cout << "========================================\n";
    
    // Create test memory and CPU
    SimpleTestMemory memory;
    fam65xx_cpp::fam65xx<config_6502> cpu;
    
    std::cout << "Initializing CPU with reset asserted...\n";
    
    // Initialize CPU which starts with reset pending
    cpu.init();
    
    std::cout << "Starting CPU cycles...\n";
    
    // Run CPU for several cycles to see if reset sequence works
    for (int cycle = 0; cycle < 30; cycle++) {
        // Create bus state for this cycle
        bus_state_t bus_state = 0;
        
        // Set up basic bus control lines
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY high (ready)
        bus_state |= BUS_BIT(BUS_RW_BIT);   // Default to read mode
        
        // Get the address the CPU wants to access first
        bus_state = cpu.cycle_tick(bus_state);
        uint16_t addr = BUS_GET_ADDR(bus_state);
        
        // Respond to CPU memory operations
        if (bus_state & BUS_BIT(BUS_RW_BIT)) {
            // Read cycle - provide data to CPU
            uint8_t data = memory.read(addr);
            BUS_SET_DATA(bus_state, data);
            
            // Debug: Show what data we're providing for specific addresses
            if (addr == 0xFFFC || addr == 0xFFFD) {
                std::cout << "  DEBUG: Pre-loading reset vector $" << std::hex << addr
                          << " -> $" << std::setw(2) << (int)data << std::endl;
            }
            
            // Debug: Show data operation details
            if (addr >= 0x1000 && addr <= 0x1010) {
                std::cout << "  DEBUG: Reading program memory $" << std::hex << addr
                          << " -> $" << std::setw(2) << (int)data << std::endl;
            }
        } else {
            // Write cycle - CPU wants to write to memory
            uint8_t data = BUS_GET_DATA(bus_state);
            memory.write(addr, data);
            std::cout << "  DEBUG: Writing to $" << std::hex << addr
                      << " <- $" << std::setw(2) << (int)data << std::endl;
        }
        
        // Get CPU state for monitoring
        auto pc = cpu.get_pc();
        auto a = cpu.get_a();
        auto cycle_step = cpu.get_cycle_step();
        auto opcode = cpu.get_opcode();
        
        std::cout << "Cycle " << std::setw(2) << cycle
                  << ": PC=$" << std::hex << std::setfill('0') << std::setw(4) << pc
                  << ", A=$" << std::setw(2) << (int)a
                  << ", Op=$" << std::setw(2) << opcode
                  << ", Step=" << std::dec << (int)cycle_step
                  << ", Addr=$" << std::hex << std::setw(4) << addr << std::endl;
        
        // Check if we've reached our test program and executed LDA
        if (pc >= 0x1000 && a == 0x42) {
            std::cout << "\n✅ SUCCESS: CPU successfully executed reset sequence and test program!\n";
            std::cout << "   - Reset vector reading worked\n";
            std::cout << "   - PC correctly set to $" << std::hex << pc << "\n";
            std::cout << "   - LDA #$42 instruction executed correctly\n";
            std::cout << "   - A register contains expected value $42\n";
            return 0;
        }
        
        // Check if PC is stuck at 0 after some cycles
        if (cycle > 15 && pc == 0x0000) {
            std::cout << "\n❌ FAILURE: CPU stuck at PC=$0000 - reset vector reading failed!\n";
            return 1;
        }
    }
    
    std::cout << "\n❌ FAILURE: Test did not complete as expected\n";
    std::cout << "Final PC: $" << std::hex << cpu.get_pc() << std::endl;
    return 1;
}