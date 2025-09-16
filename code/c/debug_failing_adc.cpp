#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== Debugging Failing ADC Test Case ===" << std::endl;
    
    // Test the failing case we observed: "69 0a e1"
    // This means ADC #$0a with some unknown initial state
    // We know: Expected A = 0x13, Got A = 0xd
    
    // Let's work backwards: if ADC #$0a should produce 0x13, what was the initial A?
    // If A_initial + 0x0a + carry = 0x13, then:
    // Case 1: A_initial + 0x0a = 0x13, so A_initial = 0x13 - 0x0a = 0x09 (no carry)
    // Case 2: A_initial + 0x0a + 1 = 0x13, so A_initial = 0x13 - 0x0a - 1 = 0x08 (with carry)
    
    std::cout << "Testing possible initial states for failing case..." << std::endl;
    
    // Test Case 1: A=0x09, carry=0
    {
        fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
        uint8_t memory[65536];
        std::fill(memory, memory + 65536, 0);
        
        cpu.init_for_test();
        cpu.set_pc(0x5afa);  // PC from failing test
        cpu.set_a(0x09);     // Hypothetical initial A
        cpu.set_status(0xe0); // Clear carry (bit 0)
        
        memory[0x5afa] = 0x69; // ADC immediate
        memory[0x5afb] = 0x0a; // operand
        
        std::cout << "\nTest Case 1 - A=0x09, Carry=0:" << std::endl;
        std::cout << "Initial: A=0x" << std::hex << (int)cpu.get_a() 
                  << " P=0x" << (int)cpu.get_status() 
                  << " Carry=" << ((cpu.get_status() & 1) ? 1 : 0) << std::endl;
        
        // Execute ADC instruction
        for (int cycles = 0; cycles < 10; cycles++) {
            uint16_t addr = cpu.get_address();
            bool is_write = !cpu.get_rw();
            
            bus_state_t bus_state = 0;
            if (is_write) {
                uint8_t data = cpu.get_write_data();
                memory[addr] = data;
                BUS_SET_DATA(bus_state, data);
            } else {
                uint8_t data = memory[addr];
                BUS_SET_DATA(bus_state, data);
            }
            bus_state |= BUS_BIT(BUS_RDY_BIT);
            
            bus_state = cpu.cycle_tick(bus_state);
            
            if (cpu.get_cycle_step() == 0 && cycles > 0) break;
        }
        
        std::cout << "Result: A=0x" << std::hex << (int)cpu.get_a() 
                  << " P=0x" << (int)cpu.get_status() << std::endl;
        std::cout << "Expected: A=0x13" << std::endl;
        std::cout << "Match: " << (cpu.get_a() == 0x13 ? "YES" : "NO") << std::endl;
    }
    
    // Test Case 2: A=0x08, carry=1
    {
        fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
        uint8_t memory[65536];
        std::fill(memory, memory + 65536, 0);
        
        cpu.init_for_test();
        cpu.set_pc(0x5afa);  // PC from failing test
        cpu.set_a(0x08);     // Hypothetical initial A
        cpu.set_status(0xe1); // Set carry (bit 0)
        
        memory[0x5afa] = 0x69; // ADC immediate
        memory[0x5afb] = 0x0a; // operand
        
        std::cout << "\nTest Case 2 - A=0x08, Carry=1:" << std::endl;
        std::cout << "Initial: A=0x" << std::hex << (int)cpu.get_a() 
                  << " P=0x" << (int)cpu.get_status() 
                  << " Carry=" << ((cpu.get_status() & 1) ? 1 : 0) << std::endl;
        
        // Execute ADC instruction
        for (int cycles = 0; cycles < 10; cycles++) {
            uint16_t addr = cpu.get_address();
            bool is_write = !cpu.get_rw();
            
            bus_state_t bus_state = 0;
            if (is_write) {
                uint8_t data = cpu.get_write_data();
                memory[addr] = data;
                BUS_SET_DATA(bus_state, data);
            } else {
                uint8_t data = memory[addr];
                BUS_SET_DATA(bus_state, data);
            }
            bus_state |= BUS_BIT(BUS_RDY_BIT);
            
            bus_state = cpu.cycle_tick(bus_state);
            
            if (cpu.get_cycle_step() == 0 && cycles > 0) break;
        }
        
        std::cout << "Result: A=0x" << std::hex << (int)cpu.get_a() 
                  << " P=0x" << (int)cpu.get_status() << std::endl;
        std::cout << "Expected: A=0x13" << std::endl;
        std::cout << "Match: " << (cpu.get_a() == 0x13 ? "YES" : "NO") << std::endl;
    }
    
    // Test what our CPU produces with A=0x09, carry=0 (should be 0x13)
    std::cout << "\n=== Manual calculation verification ===" << std::endl;
    std::cout << "A=0x09 + 0x0a + carry=0 = " << std::hex << (0x09 + 0x0a + 0) << " (expected: 0x13)" << std::endl;
    std::cout << "A=0x08 + 0x0a + carry=1 = " << std::hex << (0x08 + 0x0a + 1) << " (expected: 0x13)" << std::endl;
    
    return 0;
}