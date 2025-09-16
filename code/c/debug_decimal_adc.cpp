#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== Debugging Decimal Mode ADC ===" << std::endl;
    
    // Test the exact failing case: "69 0a e1"
    // Initial: a=2, p=175 (0xAF, includes decimal mode bit), carry=1
    // ADC #$0a
    // Expected: a=19, p=44
    
    fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];
    std::fill(memory, memory + 65536, 0);
    
    cpu.init_for_test();
    cpu.set_pc(23290);   // PC from test
    cpu.set_a(2);        // Initial A
    cpu.set_status(175); // P = 0xAF (includes decimal mode bit 3)
    
    memory[23290] = 0x69; // ADC immediate
    memory[23291] = 0x0a; // operand = 10
    
    std::cout << "Exact ProcessorTests case:" << std::endl;
    std::cout << "Initial: A=" << (int)cpu.get_a() 
              << " P=" << (int)cpu.get_status() 
              << " (0x" << std::hex << (int)cpu.get_status() << ")" << std::endl;
    
    // Check decimal mode flag
    bool decimal_mode = (cpu.get_status() & 0x08) != 0;
    bool carry_set = (cpu.get_status() & 0x01) != 0;
    std::cout << "Decimal mode: " << (decimal_mode ? "YES" : "NO") << std::endl;
    std::cout << "Carry set: " << (carry_set ? "YES" : "NO") << std::endl;
    
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
    
    std::cout << "Result: A=" << std::dec << (int)cpu.get_a() 
              << " P=" << (int)cpu.get_status() 
              << " (0x" << std::hex << (int)cpu.get_status() << ")" << std::endl;
    std::cout << "Expected: A=19 P=44 (0x2C)" << std::endl;
    
    std::cout << "\nA Match: " << (cpu.get_a() == 19 ? "YES" : "NO") << std::endl;
    std::cout << "P Match: " << (cpu.get_status() == 44 ? "YES" : "NO") << std::endl;
    
    // Manual BCD calculation
    std::cout << "\n=== Manual BCD calculation ===" << std::endl;
    std::cout << "BCD: 02 + 10 + 1 (carry) = ?" << std::endl;
    std::cout << "Step 1: 02 + 10 = 12 BCD" << std::endl;
    std::cout << "Step 2: 12 + 1 = 13 BCD = 19 decimal" << std::endl;
    std::cout << "Expected result: 19 (0x13)" << std::endl;
    
    return 0;
}