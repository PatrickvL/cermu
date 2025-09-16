#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>
#include <cstdint>

using namespace fam65xx_cpp;

int main() {
    // Test the exact failing case using ProcessorTests execution style
    fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];
    
    // Clear memory
    std::fill(memory, memory + 65536, 0);
    
    // Use the SAME initialization as ProcessorTests
    cpu.init_for_test();
    
    // Set initial state exactly as in the failing test "a9 b2 cb"
    cpu.set_pc(19094);  // 0x4a96
    cpu.set_s(237);
    cpu.set_a(178);
    cpu.set_x(129);
    cpu.set_y(142);
    cpu.set_status(162);  // 0xa2 - initial P register
    
    // Set up memory like ProcessorTests
    memory[19094] = 0xa9;  // LDA immediate
    memory[19095] = 0xb2;  // operand (178)
    memory[19096] = 0xcb;  // next instruction
    
    std::cout << "=== ProcessorTests-Style LDA Test ===\n";
    std::cout << "--- Initial State ---\n";
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    
    // Execute using the EXACT ProcessorTests pattern
    uint32_t initial_cycles = cpu.get_cycle_count();
    
    for (int max_cycles = 0; max_cycles < 10; max_cycles++) {
        std::cout << "\n--- Cycle " << max_cycles << " ---\n";
        
        // Get address BEFORE cycle execution (ProcessorTests pattern)
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        std::cout << "Address: 0x" << std::hex << addr << std::endl;
        std::cout << "RW: " << (is_write ? "WRITE" : "READ") << std::endl;
        
        bus_state_t bus_state = 0;
        if (is_write) {
            // Write cycle
            uint8_t data = cpu.get_write_data();
            std::cout << "Write data: 0x" << std::hex << (int)data << std::endl;
            memory[addr] = data;
            BUS_SET_DATA(bus_state, data);
        } else {
            // Read cycle
            uint8_t data = memory[addr];
            std::cout << "Read data: 0x" << std::hex << (int)data << std::endl;
            BUS_SET_DATA(bus_state, data);
        }
        
        // Set RDY line (ready)
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        std::cout << "Before cycle_tick: PC=0x" << std::hex << cpu.get_pc() 
                  << ", A=0x" << (int)cpu.get_a() << ", P=0x" << (int)cpu.get_status() << std::endl;
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "After cycle_tick: PC=0x" << std::hex << cpu.get_pc() 
                  << ", A=0x" << (int)cpu.get_a() << ", P=0x" << (int)cpu.get_status() << std::endl;
        std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
        
        // Check if instruction completed (cycle_step wrapped back to 0)
        if (cpu.get_cycle_step() == 0 && max_cycles > 0) {
            std::cout << "Instruction completed!" << std::endl;
            break;
        }
    }
    
    uint32_t cycles_executed = cpu.get_cycle_count() - initial_cycles;
    
    std::cout << "\n--- Final Results ---\n";
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "Cycles: " << std::dec << cycles_executed << std::endl;
    
    std::cout << "\n--- Comparison with Expected ---\n";
    std::cout << "Expected A: 0xb2, Got A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "Expected P: 0xa0, Got P: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    
    if (cpu.get_a() == 0xb2 && cpu.get_status() == 0xa0) {
        std::cout << "✅ TEST PASSED - Matches ProcessorTests expectation!" << std::endl;
    } else {
        std::cout << "❌ TEST FAILED - Does not match ProcessorTests expectation!" << std::endl;
    }
    
    return 0;
}