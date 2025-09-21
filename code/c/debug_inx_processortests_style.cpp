#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/core/system_lines.h"
#include <cstdio>
#include <cstdint>

using namespace fam65xx_cpp;

int main() {
    printf("=== INX ProcessorTests Style Debug ===\n");
    
    // Create CPU instance - use same configuration as ProcessorTests
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state similar to ProcessorTests test cases
    cpu.set_x(0x00);  // Start with X = 0x00
    cpu.set_pc(0x0000);
    cpu.set_p(0x24);  // Standard flags
    
    printf("Initial state: X=0x%02X, P=0x%02X, PC=0x%04X\n", 
           cpu.get_x(), cpu.get_p(), cpu.get_pc());
    
    // Set up memory simulation - ProcessorTests style
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Set RDY line HIGH
    
    // Execute INX instruction like ProcessorTests would
    printf("\n=== Cycle-by-cycle execution ===\n");
    
    // Cycle 1: Opcode fetch
    printf("Cycle 1: Address=0x%04X, RW=%s\n", 
           cpu.get_address(), cpu.get_rw() ? "READ" : "WRITE");
    bus_state = BUS_SET_DATA(bus_state, 0xE8);  // INX opcode
    bus_state = cpu.cycle_tick(bus_state);
    printf("After cycle 1: opcode=0x%02X, step=%d, X=0x%02X, P=0x%02X\n", 
           cpu.get_opcode(), cpu.get_cycle_step(), cpu.get_x(), cpu.get_p());
    
    // Cycle 2: INX execution 
    printf("Cycle 2: Address=0x%04X, RW=%s\n", 
           cpu.get_address(), cpu.get_rw() ? "READ" : "WRITE");
    bus_state = BUS_SET_DATA(bus_state, 0x00);  // Dummy read data
    bus_state = cpu.cycle_tick(bus_state);
    printf("After cycle 2: opcode=0x%02X, step=%d, X=0x%02X, P=0x%02X\n", 
           cpu.get_opcode(), cpu.get_cycle_step(), cpu.get_x(), cpu.get_p());
    
    // Check final state
    printf("\n=== Final Results ===\n");
    printf("Final X: 0x%02X (expected: 0x01)\n", cpu.get_x());
    printf("Final P: 0x%02X\n", cpu.get_p());
    printf("Z flag: %d (expected: 0 since result != 0)\n", (cpu.get_p() & 0x02) ? 1 : 0);
    printf("N flag: %d (expected: 0 since result < 0x80)\n", (cpu.get_p() & 0x80) ? 1 : 0);
    
    // Verify results
    bool x_correct = (cpu.get_x() == 0x01);
    bool z_correct = (cpu.get_p() & 0x02) == 0;  // Z should be clear
    bool n_correct = (cpu.get_p() & 0x80) == 0;  // N should be clear
    
    printf("\nTest results:\n");
    printf("X register: %s\n", x_correct ? "CORRECT" : "INCORRECT");
    printf("Z flag: %s\n", z_correct ? "CORRECT" : "INCORRECT");
    printf("N flag: %s\n", n_correct ? "CORRECT" : "INCORRECT");
    
    if (x_correct && z_correct && n_correct) {
        printf("\n✅ ProcessorTests style INX test PASSED\n");
        return 0;
    } else {
        printf("\n❌ ProcessorTests style INX test FAILED\n");
        return 1;
    }
}