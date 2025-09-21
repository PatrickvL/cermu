#include "fam65xx.hpp"
#include "system_lines.h"
#include <cstdio>
#include <cstdint>

using namespace fam65xx_cpp;

int main() {
    printf("=== INX (0xE8) Simple Debug Test ===\n");
    
    // Create CPU instance
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state for INX test
    cpu.set_x(0x7F);  // X = 0x7F, so INX should make it 0x80 (negative)
    cpu.set_pc(0x1000);
    cpu.set_p(0x24);  // Clear all flags except unused
    
    printf("Before INX: X=0x%02X, P=0x%02X\n", cpu.get_x(), cpu.get_p());
    
    // Set up memory with INX instruction at PC
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Set RDY line HIGH
    
    // Cycle 0: Opcode fetch (should read 0xE8)
    printf("\n--- Cycle 0: Opcode fetch ---\n");
    bus_state = BUS_SET_DATA(bus_state, 0xE8);  // INX opcode
    printf("Bus data set to 0xE8 (INX)\n");
    printf("CPU address: 0x%04X\n", cpu.get_address());
    printf("CPU RW: %s\n", cpu.get_rw() ? "READ" : "WRITE");
    
    bus_state = cpu.cycle_tick(bus_state);
    printf("After cycle 0: opcode=0x%02X, step=%d\n", cpu.get_opcode(), cpu.get_cycle_step());
    printf("PC: 0x%04X, X: 0x%02X, P: 0x%02X\n", cpu.get_pc(), cpu.get_x(), cpu.get_p());
    
    // Cycle 1: INX execution
    printf("\n--- Cycle 1: INX execution ---\n");
    printf("CPU address: 0x%04X\n", cpu.get_address());
    printf("CPU RW: %s\n", cpu.get_rw() ? "READ" : "WRITE");
    
    // INX is a 2-cycle instruction that reads the next byte but ignores it
    bus_state = BUS_SET_DATA(bus_state, 0x00);  // Dummy data, should be ignored
    
    bus_state = cpu.cycle_tick(bus_state);
    printf("After cycle 1: opcode=0x%02X, step=%d\n", cpu.get_opcode(), cpu.get_cycle_step());
    printf("PC: 0x%04X, X: 0x%02X, P: 0x%02X\n", cpu.get_pc(), cpu.get_x(), cpu.get_p());
    
    // Check expected results
    printf("\n=== Results Analysis ===\n");
    printf("Expected X: 0x80 (0x7F + 1)\n");
    printf("Actual X: 0x%02X\n", cpu.get_x());
    printf("Expected P flags: N=1, Z=0 (result 0x80 is negative, non-zero)\n");
    printf("Actual P: 0x%02X (N=%d, Z=%d)\n", 
           cpu.get_p(), 
           (cpu.get_p() & 0x80) ? 1 : 0,  // N flag
           (cpu.get_p() & 0x02) ? 1 : 0); // Z flag
    
    // Test success conditions
    bool x_correct = (cpu.get_x() == 0x80);
    bool n_flag_correct = (cpu.get_p() & 0x80) != 0;  // N should be set (0x80 is negative)
    bool z_flag_correct = (cpu.get_p() & 0x02) == 0;  // Z should be clear (0x80 is not zero)
    
    printf("\nTest Results:\n");
    printf("X register: %s\n", x_correct ? "CORRECT" : "INCORRECT");
    printf("N flag: %s\n", n_flag_correct ? "CORRECT" : "INCORRECT");
    printf("Z flag: %s\n", z_flag_correct ? "CORRECT" : "INCORRECT");
    
    if (x_correct && n_flag_correct && z_flag_correct) {
        printf("\n✅ INX TEST PASSED\n");
        return 0;
    } else {
        printf("\n❌ INX TEST FAILED\n");
        return 1;
    }
}