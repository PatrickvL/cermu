#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== ProcessorTests Integration Debug ===" << std::endl;
    
    // Create CPU exactly like ProcessorTests runner
    fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];
    
    // Clear memory
    std::fill(memory, memory + 65536, 0);
    
    // Initialize for testing (not reset)
    cpu.init_for_test();
    
    // Set up the exact state from the failing test: 69 1b 91
    cpu.set_pc(0xbfdf);  // PC from test
    cpu.set_a(0x4c);     // A = 76 (0x4c)
    cpu.set_x(0x91);     // X from test
    cpu.set_y(0x1b);     // Y from test  
    cpu.set_sp(0x91);    // SP from test
    cpu.set_status(0xe4); // P = 228 (0xe4)
    
    // Set up memory with the instruction sequence
    memory[0xbfdf] = 0x69; // ADC immediate
    memory[0xbfe0] = 0x1b; // operand = 27 (0x1b)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "A = " << (int)cpu.get_a() << " (0x" << std::hex << (int)cpu.get_a() << ")" << std::endl;
    std::cout << "P = " << std::dec << (int)cpu.get_status() << " (0x" << std::hex << (int)cpu.get_status() << ")" << std::endl;
    std::cout << "PC = 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Carry = " << ((cpu.get_status() & 0x01) ? 1 : 0) << std::endl;
    
    // Execute the instruction using cycle_tick exactly like ProcessorTests
    std::cout << "\nExecuting ADC #$1b instruction using cycle_tick..." << std::endl;
    
    uint32_t initial_cycles = cpu.get_cycle_count();
    uint16_t initial_pc = cpu.get_pc();
    uint8_t initial_step = cpu.get_cycle_step();
    
    // Execute cycles with bus interface (exactly like ProcessorTests runner)
    for (int max_cycles = 0; max_cycles < 10; max_cycles++) {
        std::cout << "Cycle " << max_cycles << ": ";
        std::cout << "PC=0x" << std::hex << cpu.get_pc() << " ";
        std::cout << "Step=" << std::dec << (int)cpu.get_cycle_step() << " ";
        
        // Create bus state for reading
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        std::cout << "Addr=0x" << std::hex << addr << " ";
        std::cout << (is_write ? "WRITE" : "READ") << " ";
        
        bus_state_t bus_state = 0;
        if (is_write) {
            // Write cycle
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            BUS_SET_DATA(bus_state, data);
            std::cout << "Data=0x" << std::hex << (int)data << " ";
        } else {
            // Read cycle
            uint8_t data = memory[addr];
            BUS_SET_DATA(bus_state, data);
            std::cout << "Data=0x" << std::hex << (int)data << " ";
        }
        
        // Set RDY line (ready)
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "A=0x" << std::hex << (int)cpu.get_a() << " ";
        std::cout << "P=0x" << std::hex << (int)cpu.get_status() << std::endl;
        
        // Check if instruction completed (cycle_step wrapped back to 0)
        if (cpu.get_cycle_step() == 0 && max_cycles > 0) {
            std::cout << "Instruction completed after " << (max_cycles + 1) << " cycles" << std::endl;
            break;
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "A = " << std::dec << (int)cpu.get_a() << " (0x" << std::hex << (int)cpu.get_a() << ")" << std::endl;
    std::cout << "P = " << std::dec << (int)cpu.get_status() << " (0x" << std::hex << (int)cpu.get_status() << ")" << std::endl;
    std::cout << "PC = 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Cycles executed = " << std::dec << (cpu.get_cycle_count() - initial_cycles) << std::endl;
    
    std::cout << "\nExpected results:" << std::endl;
    std::cout << "A = 103 (0x67)" << std::endl;
    std::cout << "P = 36 (0x24)" << std::endl;
    std::cout << "PC = 0xbfe1" << std::endl;
    
    std::cout << "\nComparison:" << std::endl;
    std::cout << "A Match: " << (cpu.get_a() == 0x67 ? "YES" : "NO") << std::endl;
    std::cout << "P Match: " << (cpu.get_status() == 0x24 ? "YES" : "NO") << std::endl;
    std::cout << "PC Match: " << (cpu.get_pc() == 0xbfe1 ? "YES" : "NO") << std::endl;
    
    return 0;
}