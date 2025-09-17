#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

void test_tax_with_flags(uint8_t initial_a, uint8_t initial_flags, const char* test_name) {
    std::cout << "\n=== " << test_name << " ===" << std::endl;
    
    // Create CPU with 6502 configuration
    fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];
    
    // Initialize memory
    std::fill(memory, memory + 65536, 0);
    
    // Use test initialization (no reset pending)
    cpu.init_for_test();
    
    // Set test values
    cpu.set_a(initial_a);
    cpu.set_x(0x00);      // Clear X register
    cpu.set_status(initial_flags); // Set initial flags
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "A = 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "X = 0x" << std::hex << (int)cpu.get_x() << std::endl;
    std::cout << "P = 0x" << std::hex << (int)cpu.get_status() << " (";
    if (cpu.get_status() & P_NEGATIVE) std::cout << "N";
    if (cpu.get_status() & P_ZERO) std::cout << "Z";
    if (cpu.get_status() & P_CARRY) std::cout << "C";
    if (cpu.get_status() & P_OVERFLOW) std::cout << "V";
    std::cout << ")" << std::endl;
    
    // Set up memory with TAX instruction at address 0x0000
    // TAX opcode is 0xAA
    memory[0x0000] = 0xAA;
    memory[0x0001] = 0xEA; // NOP for next instruction
    
    // Set PC to start of program
    cpu.set_pc(0x0000);
    
    std::cout << "\nExecuting TAX instruction (0xAA)..." << std::endl;
    
    // Execute the TAX instruction step by step
    bool instruction_complete = false;
    int cycle_count = 0;
    
    while (!instruction_complete && cycle_count < 5) {
        std::cout << "Cycle " << cycle_count << ":" << std::endl;
        std::cout << "  PC = 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << cpu.get_pc() << std::endl;
        std::cout << "  Step = " << (int)cpu.get_cycle_step() << std::endl;
        
        // Create bus state for cycle execution
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        bus_state_t bus_state = 0;
        if (is_write) {
            // Write cycle
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            BUS_SET_DATA(bus_state, data);
            std::cout << "  Writing 0x" << std::hex << (int)data << " to 0x" << addr << std::endl;
        } else {
            // Read cycle
            uint8_t data = memory[addr];
            BUS_SET_DATA(bus_state, data);
            std::cout << "  Reading 0x" << std::hex << (int)data << " from 0x" << addr << std::endl;
        }
        
        // Set RDY line (ready)
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        // Check if instruction completed before this cycle
        uint8_t step_before = cpu.get_cycle_step();
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction completed (step wrapped back to 0)
        uint8_t step_after = cpu.get_cycle_step();
        if (step_before > 0 && step_after == 0) {
            instruction_complete = true;
            std::cout << "  Instruction completed!" << std::endl;
        }
        
        // Show state after cycle
        std::cout << "  After cycle: A=0x" << std::hex << (int)cpu.get_a() 
                  << ", X=0x" << (int)cpu.get_x() 
                  << ", P=0x" << (int)cpu.get_status() << std::endl;
        
        cycle_count++;
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "A = 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "X = 0x" << std::hex << (int)cpu.get_x() << std::endl;
    std::cout << "P = 0x" << std::hex << (int)cpu.get_status() << " (";
    if (cpu.get_status() & P_NEGATIVE) std::cout << "N";
    if (cpu.get_status() & P_ZERO) std::cout << "Z";
    if (cpu.get_status() & P_CARRY) std::cout << "C";
    if (cpu.get_status() & P_OVERFLOW) std::cout << "V";
    std::cout << ")" << std::endl;
    
    // Expected results for TAX:
    // - X should equal A
    // - N flag should be set if bit 7 of A is set
    // - Z flag should be set if A is zero
    // - C and V flags should be preserved
    uint8_t expected_x = initial_a;
    uint8_t expected_flags = (initial_flags & ~(P_NEGATIVE | P_ZERO)) |
                            (initial_a & 0x80 ? P_NEGATIVE : 0) |
                            (initial_a == 0 ? P_ZERO : 0);
    
    bool x_correct = (cpu.get_x() == expected_x);
    bool flags_correct = (cpu.get_status() == expected_flags);
    
    std::cout << "\nExpected: X=0x" << std::hex << (int)expected_x 
              << ", P=0x" << (int)expected_flags << std::endl;
    std::cout << "Got:      X=0x" << std::hex << (int)cpu.get_x() 
              << ", P=0x" << (int)cpu.get_status() << std::endl;
    
    if (x_correct && flags_correct) {
        std::cout << "✅ TEST PASSED" << std::endl;
    } else {
        std::cout << "❌ TEST FAILED" << std::endl;
        if (!x_correct) std::cout << "  X register mismatch" << std::endl;
        if (!flags_correct) std::cout << "  Flag mismatch" << std::endl;
    }
}

int main() {
    std::cout << "=== TAX Instruction Detailed Debug Test ===" << std::endl;
    
    // Test different scenarios that ProcessorTests might cover
    test_tax_with_flags(0x42, 0x00, "TAX with positive value, clear flags");
    test_tax_with_flags(0x00, 0x00, "TAX with zero value, clear flags");
    test_tax_with_flags(0x80, 0x00, "TAX with negative value, clear flags");
    test_tax_with_flags(0x42, P_CARRY | P_OVERFLOW, "TAX with positive value, preserve C+V flags");
    test_tax_with_flags(0x00, P_NEGATIVE | P_CARRY, "TAX with zero value, should clear N flag");
    test_tax_with_flags(0xFF, 0x00, "TAX with $FF (negative + non-zero)");
    
    return 0;
}