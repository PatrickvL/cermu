#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

void test_jump_call_instruction(uint8_t opcode, const char* name) {
    printf("\n=== Testing %s (0x%02X) ===\n", name, opcode);
    
    // Create CPU instance using the same config as ProcessorTests
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.reset();
    
    // Set up initial state
    uint32_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // CRITICAL: Set RDY line HIGH
    bus_state |= BUS_BIT(BUS_SO_BIT);   // Set SO line HIGH (not overflow)
    
    // Simple test - just see if instruction completes without crashing
    cpu.set_program_counter(0x8000);
    
    // Set up memory with the instruction
    if (opcode == 0x4C) {
        // JMP $1234 (absolute)
        cpu.set_memory_byte(0x8000, 0x4C);
        cpu.set_memory_byte(0x8001, 0x34);
        cpu.set_memory_byte(0x8002, 0x12);
    } else if (opcode == 0x6C) {
        // JMP ($1234) (indirect)
        cpu.set_memory_byte(0x8000, 0x6C);
        cpu.set_memory_byte(0x8001, 0x34);
        cpu.set_memory_byte(0x8002, 0x12);
        // Set up indirect target
        cpu.set_memory_byte(0x1234, 0x78);
        cpu.set_memory_byte(0x1235, 0x56);
    } else if (opcode == 0x40) {
        // RTI
        cpu.set_memory_byte(0x8000, 0x40);
        // Set up stack with return address and status
        cpu.set_stack_pointer(0xFC);  // SP will be incremented during RTI
        cpu.set_memory_byte(0x01FD, 0x30);  // Status register
        cpu.set_memory_byte(0x01FE, 0x34);  // PC low
        cpu.set_memory_byte(0x01FF, 0x12);  // PC high
    }
    
    // Test execution
    int cycles = 0;
    bool completed = false;
    uint16_t initial_pc = cpu.get_program_counter();
    
    printf("Initial PC: 0x%04X\n", initial_pc);
    
    // Execute the instruction
    while (cycles < 20 && !completed) {  // Safety limit
        bus_state = cpu.cycle_tick(bus_state);
        cycles++;
        
        // Check if instruction completed by seeing if PC changed appropriately
        uint16_t current_pc = cpu.get_program_counter();
        
        if (opcode == 0x4C && current_pc == 0x1234) {
            completed = true;
            printf("JMP absolute completed successfully! PC = 0x%04X\n", current_pc);
        } else if (opcode == 0x6C && current_pc == 0x5678) {
            completed = true;
            printf("JMP indirect completed successfully! PC = 0x%04X\n", current_pc);
        } else if (opcode == 0x40 && current_pc == 0x1234) {
            completed = true;
            printf("RTI completed successfully! PC = 0x%04X\n", current_pc);
        }
        
        // Also check if we're back in sync mode (instruction completed)
        if ((bus_state & BUS_BIT(BUS_SYNC_BIT)) && cycles > 1) {
            if (!completed) {
                printf("Instruction completed after %d cycles, PC = 0x%04X\n", cycles, current_pc);
                completed = true;
            }
        }
    }
    
    if (!completed) {
        printf("ERROR: Instruction did not complete within %d cycles\n", cycles);
        printf("Final PC: 0x%04X\n", cpu.get_program_counter());
    }
    
    printf("Total cycles: %d\n", cycles);
}

int main() {
    printf("=== Jump/Call Instructions Validation ===\n");
    
    // Test the jump/call instructions
    test_jump_call_instruction(0x4C, "JMP absolute");
    test_jump_call_instruction(0x6C, "JMP indirect");
    test_jump_call_instruction(0x40, "RTI");
    
    printf("\n=== Jump/Call Validation Complete ===\n");
    return 0;
}