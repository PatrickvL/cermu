#include "tests/fam65xx_cpp_test_harness.h"
#include <cstdio>

int main() {
    printf("=== RTI REGRESSION ANALYSIS ===\n");
    
    // Create test CPU
    auto cpu = create_test_cpu();
    uint8_t memory[65536] = {0};
    
    // Set up a basic RTI test case
    cpu.set_pc(0x8000);
    cpu.set_s(0xFD);   // Stack pointer
    cpu.set_p(0x04);   // Initial P register (U flag set)
    
    // Set up stack for RTI (similar to our working simple test)
    memory[0x01FE] = 0x34;  // Status register with B=1, U=1 (should become B=0, U=1)
    memory[0x01FF] = 0xAA;  // PC low
    memory[0x0100] = 0x65;  // PC high
    
    // Set RTI opcode
    memory[0x8000] = 0x40;
    
    printf("BEFORE RTI:\n");
    printf("  PC=0x%04X, SP=0x%02X, P=0x%02X\n", cpu.get_pc(), cpu.get_s(), cpu.get_p());
    printf("  Stack: [0x01FE]=0x%02X, [0x01FF]=0x%02X, [0x0100]=0x%02X\n", 
           memory[0x01FE], memory[0x01FF], memory[0x0100]);
    
    // Execute RTI instruction
    int cycles = 0;
    bool rti_executing = true;
    
    while (rti_executing && cycles < 10) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        printf("Cycle %d: addr=0x%04X, rw=%s, opcode=0x%02X, step=%d\n", 
               cycles + 1, addr, is_write ? "WRITE" : "READ", cpu.get_opcode(), cpu.get_cycle_step());
        
        if (is_write) {
            memory[addr] = cpu.get_write_data();
            printf("  Write data: 0x%02X to 0x%04X\n", cpu.get_write_data(), addr);
        }
        
        uint8_t data = memory[addr];
        printf("  Read data: 0x%02X from 0x%04X\n", data, addr);
        
        bus_state_t bus_state = create_bus_state(data);
        bus_state = cpu.cycle_tick(bus_state);
        cycles++;
        
        // Check if RTI completed
        if (cpu.get_opcode() != 0x40 || (cpu.get_opcode() == 0x40 && cpu.get_cycle_step() == 0)) {
            rti_executing = false;
        }
        
        printf("  After cycle: PC=0x%04X, SP=0x%02X, P=0x%02X, opcode=0x%02X, step=%d\n", 
               cpu.get_pc(), cpu.get_s(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
        printf("\n");
    }
    
    printf("AFTER RTI:\n");
    printf("  PC=0x%04X, SP=0x%02X, P=0x%02X\n", cpu.get_pc(), cpu.get_s(), cpu.get_p());
    printf("  Cycles executed: %d\n", cycles);
    
    // Expected results
    printf("\nEXPECTED:\n");
    printf("  PC=0x65AA, SP=0x00, P=0x24 (B=0, U=1)\n");
    
    // Validate results
    bool pc_correct = (cpu.get_pc() == 0x65AA);
    bool sp_correct = (cpu.get_s() == 0x00);
    bool p_correct = (cpu.get_p() == 0x24);
    
    printf("\nVALIDATION:\n");
    printf("  PC: %s (expected 0x65AA, got 0x%04X)\n", pc_correct ? "PASS" : "FAIL", cpu.get_pc());
    printf("  SP: %s (expected 0x00, got 0x%02X)\n", sp_correct ? "PASS" : "FAIL", cpu.get_s());
    printf("  P:  %s (expected 0x24, got 0x%02X)\n", p_correct ? "PASS" : "FAIL", cpu.get_p());
    
    if (pc_correct && sp_correct && p_correct) {
        printf("\n🎉 RTI BASIC TEST PASSED! 🎉\n");
        return 0;
    } else {
        printf("\n❌ RTI BASIC TEST FAILED!\n");
        return 1;
    }
}