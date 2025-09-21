#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include <stdio.h>

using namespace fam65xx_cpp;

int main() {
    auto cpu = fam65xx_with_cycle_count<config_6502>();
    cpu.init_for_test();
    
    // Set up the exact failing test case from verbose output
    // FAIL 4c 2a cd: P - expected 0x26, got 0x66
    
    // Set initial state to match the failing test
    cpu.set_p(0x26);  // Expected final P value
    cpu.set_pc(0x3b72);  // From verbose: "Opcode at PC 0x3b72: 0x4c"
    
    printf("Before JMP: P=0x%02X PC=0x%04X\n", cpu.get_p(), cpu.get_pc());
    
    // Simulate the memory containing JMP $CD2A instruction at PC
    bus_state_t bus_state = 0;
    
    // Cycle 0: Opcode fetch (0x4C)
    bus_state = BUS_SET_DATA(bus_state, 0x4C);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 1: Read low byte (0x2A)
    bus_state = BUS_SET_DATA(bus_state, 0x2A);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 2: Read high byte (0xCD)
    bus_state = BUS_SET_DATA(bus_state, 0xCD);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 3: Execute jump (no memory access)
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("After JMP: P=0x%02X PC=0x%04X\n", cpu.get_p(), cpu.get_pc());
    
    // Check if V flag was corrupted
    uint8_t final_p = cpu.get_p();
    if ((final_p & P_OVERFLOW) && !(0x26 & P_OVERFLOW)) {
        printf("ERROR: V flag incorrectly set! Expected P=0x26, got P=0x%02X\n", final_p);
        printf("Difference: 0x%02X (V flag = bit 6 = 0x40)\n", final_p ^ 0x26);
        return 1;
    } else {
        printf("SUCCESS: JMP instruction preserved flags correctly\n");
        return 0;
    }
}