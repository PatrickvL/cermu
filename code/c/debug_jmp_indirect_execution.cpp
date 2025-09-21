#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <cstdio>

using namespace fam65xx_cpp;

int main() {
    printf("=== JMP INDIRECT EXECUTION DEBUG ===\n");
    
    // Use config_6502 for NMOS processor
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up a simple JMP indirect test
    // JMP ($1000) where $1000 contains target address
    cpu.set_pc(0x2000);
    
    // Create a simple memory simulation for the test
    uint8_t memory[0x10000] = {0};
    
    // Set up JMP indirect instruction at $2000
    memory[0x2000] = 0x6C;  // JMP indirect opcode
    memory[0x2001] = 0x00;  // Low byte of indirect address ($1000)
    memory[0x2002] = 0x10;  // High byte of indirect address ($1000)
    
    // Set up target address at $1000
    memory[0x1000] = 0x34;  // Target low byte ($1234)
    memory[0x1001] = 0x12;  // Target high byte ($1234)
    
    printf("Initial state: PC=0x%04X, Step=%d\n", cpu.get_pc(), cpu.get_cycle_step());
    
    // Execute cycles manually
    for (int cycle = 0; cycle < 10; cycle++) {
        printf("\n--- Cycle %d ---\n", cycle);
        
        // Get the address the CPU wants to read/write
        uint16_t address = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        printf("BEFORE: PC=0x%04X, Step=%d, Address=0x%04X, R/W=%s\n",
               cpu.get_pc(), cpu.get_cycle_step(), address, is_read ? "READ" : "WRITE");
        
        // Create bus state with the data at that address
        bus_state_t bus_state = 0;
        BUS_SET_ADDR(bus_state, address);
        
        if (is_read) {
            // Provide data from our simulated memory
            uint8_t data = memory[address];
            BUS_SET_DATA(bus_state, data);
            printf("Providing data: 0x%02X from address 0x%04X\n", data, address);
        } else {
            // Get the data CPU wants to write
            uint8_t write_data = cpu.get_write_data();
            printf("CPU wants to write: 0x%02X to address 0x%04X\n", write_data, address);
        }
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        printf("AFTER:  PC=0x%04X, Step=%d\n", cpu.get_pc(), cpu.get_cycle_step());
        
        // Check if we completed the instruction
        if (cpu.get_cycle_step() == 0) {
            printf("*** INSTRUCTION COMPLETED! ***\n");
            break;
        }
        
        // Safety check - prevent infinite loops
        if (cycle > 8) {
            printf("*** TOO MANY CYCLES - BREAKING ***\n");
            break;
        }
    }
    
    printf("\nFinal state: PC=0x%04X (expected: 0x1234)\n", cpu.get_pc());
    
    return 0;
}