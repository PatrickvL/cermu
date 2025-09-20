#include "tests/fam65xx_cpp_test_harness.h"
#include <cstdio>

int main() {
    printf("=== RTI ALU Operation Check ===\n");
    
    // Create CPU and initialize for test
    auto cpu = create_test_cpu();
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_pc(0x8000);
    cpu.set_s(0xFC);
    cpu.set_p(0x24);
    
    // Set up memory for RTI instruction
    memory[0x8000] = 0x40; // RTI opcode
    
    // Set up stack with test data
    memory[0x01FD] = 0x30; // Status register
    memory[0x01FE] = 0x34; // PC low
    memory[0x01FF] = 0x12; // PC high
    
    printf("Initial state:\n");
    printf("  PC: 0x%04X\n", cpu.get_pc());
    printf("  SP: 0x%02X\n", cpu.get_s());
    printf("  P:  0x%02X\n", cpu.get_p());
    
    // Execute cycle by cycle and check ALU operations
    for (int cycle = 0; cycle < 8; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        uint8_t data = 0;
        
        if (is_write) {
            data = cpu.get_write_data();
            printf("Cycle %d: addr=0x%04X WRITE data=0x%02X opcode=0x%02X step=%d\n", 
                   cycle, addr, data, cpu.get_opcode(), cpu.get_cycle_step());
        } else {
            data = memory[addr];
            printf("Cycle %d: addr=0x%04X READ  data=0x%02X opcode=0x%02X step=%d\n", 
                   cycle, addr, data, cpu.get_opcode(), cpu.get_cycle_step());
        }
        
        // Check what ALU operation this cycle should have
        if (cpu.get_cycle_step() > 0) {
            auto cycle_desc = cpu.GET_CYCLE(cpu.get_opcode(), cpu.get_cycle_step());
            auto alu_op = static_cast<uint8_t>(cycle_desc.alu_op);
            printf("  ALU operation: %d (0=NOP)\n", alu_op);
            
            if (alu_op != 0) {
                printf("  *** WARNING: Non-NOP ALU operation detected! ***\n");
            }
        }
        
        // Execute the cycle
        tick_cpu(cpu, data);
        
        printf("  After cycle: PC=0x%04X SP=0x%02X P=0x%02X\n", 
               cpu.get_pc(), cpu.get_s(), cpu.get_p());
        
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0) {
            printf("  *** Instruction complete ***\n");
            break;
        }
    }
    
    printf("\nFinal state:\n");
    printf("  PC: 0x%04X\n", cpu.get_pc());
    printf("  SP: 0x%02X\n", cpu.get_s());
    printf("  P:  0x%02X\n", cpu.get_p());
    
    return 0;
}