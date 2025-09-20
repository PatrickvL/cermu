#include <iostream>
#include <iomanip>
#include <cstdio>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

int main() {
    printf("=== RTI EXECUTION FRAMEWORK DEBUG ===\n\n");
    
    // Use the standard MOS6502 configuration
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up RTI test scenario
    cpu.set_pc(0x8000);
    cpu.set_sp(0xFD);  // Stack pointer after interrupt pushed 3 bytes
    cpu.set_p(0x24);   // Some flags set initially
    
    printf("RTI TRACING: Initialized CPU for RTI test\n");
    printf("RTI TRACING: PC=0x%04X, SP=0x%02X, P=0x%02X\n", cpu.get_pc(), cpu.get_sp(), cpu.get_p());
    
    // Set up memory to simulate stack contents
    bus_state_t bus_state = 0;
    
    // Simulate RTI opcode fetch (0x40)
    printf("\n### STEP 1: Opcode Fetch (RTI = 0x40) ###\n");
    printf("BEFORE: PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    bus_state = BUS_SET_DATA(bus_state, 0x40);  // RTI opcode
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("AFTER:  PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    // Check if RTI was properly fetched
    if (cpu.get_opcode() != 0x40) {
        printf("ERROR: RTI opcode not fetched correctly! Expected 0x40, got 0x%02X\n", cpu.get_opcode());
        return 1;
    }
    
    // Check if we're in cycle 1
    if (cpu.get_cycle_step() != 1) {
        printf("ERROR: Should be in cycle 1, but cycle_step=%d\n", cpu.get_cycle_step());
        return 1;
    }
    
    printf("SUCCESS: RTI opcode fetched correctly, proceeding to cycle 1\n");
    
    // Set up stack data for RTI pulls
    // Stack should contain: SP+1=P, SP+2=PCL, SP+3=PCH
    printf("\n### STEP 2: RTI Cycle 1 (dummy read) ###\n");
    printf("BEFORE: PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    bus_state = BUS_SET_DATA(bus_state, 0x00);  // Dummy data
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("AFTER:  PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    // Check cycle progression
    if (cpu.get_cycle_step() == 0) {
        printf("CRITICAL ERROR: RTI completed after cycle 1! This is the bug!\n");
        printf("RTI should execute 6 cycles but completed immediately!\n");
        printf("This is the same issue we fixed for RTS - cycle execution framework bug!\n");
        return 1;
    } else if (cpu.get_cycle_step() == 2) {
        printf("SUCCESS: RTI progressed to cycle 2 correctly\n");
    } else {
        printf("ERROR: Unexpected cycle_step=%d after cycle 1\n", cpu.get_cycle_step());
        return 1;
    }
    
    printf("\n### STEP 3: RTI Cycle 2 (internal) ###\n");
    printf("BEFORE: PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    bus_state = BUS_SET_DATA(bus_state, 0x00);  // Dummy data
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("AFTER:  PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    printf("\n### STEP 4: RTI Cycle 3 (pull P) ###\n");
    printf("BEFORE: PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    bus_state = BUS_SET_DATA(bus_state, 0x30);  // Status register value
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("AFTER:  PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    // Check if P register was restored
    if (cpu.get_p() == 0x30) {
        printf("SUCCESS: P register restored correctly to 0x30\n");
    } else {
        printf("ERROR: P register not restored! Expected 0x30, got 0x%02X\n", cpu.get_p());
    }
    
    printf("\n### STEP 5: RTI Cycle 4 (pull PCL) ###\n");
    printf("BEFORE: PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    bus_state = BUS_SET_DATA(bus_state, 0xAA);  // PCL value
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("AFTER:  PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    printf("\n### STEP 6: RTI Cycle 5 (pull PCH) ###\n");
    printf("BEFORE: PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    bus_state = BUS_SET_DATA(bus_state, 0x65);  // PCH value
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("AFTER:  PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    printf("\n### STEP 7: RTI Cycle 6 (completion) ###\n");
    printf("BEFORE: PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    bus_state = BUS_SET_DATA(bus_state, 0x00);  // Dummy data
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("AFTER:  PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
           cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
    
    printf("\n=== FINAL RESULTS ===\n");
    printf("Final PC: 0x%04X (expected: 0x65AA)\n", cpu.get_pc());
    printf("Final SP: 0x%02X (expected: 0x00 - SP+3)\n", cpu.get_sp());
    printf("Final P: 0x%02X (expected: 0x30)\n", cpu.get_p());
    
    // Check if RTI worked correctly
    if (cpu.get_pc() == 0x65AA && cpu.get_p() == 0x30) {
        printf("\n*** RTI EXECUTION SUCCESS! ***\n");
        return 0;
    } else {
        printf("\n*** RTI EXECUTION FAILED! ***\n");
        printf("RTI is not executing properly - likely the same cycle execution framework issue as RTS!\n");
        return 1;
    }
}