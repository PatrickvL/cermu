#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/core/system_lines.h"
#include <cstdio>

using namespace fam65xx_cpp;

int main() {
    printf("=== JSR (Jump to Subroutine) Debug Test ===\n");
    
    // Create CPU and initialize for testing
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up test state for JSR $1234
    cpu.set_pc(0x0800);  // Start address
    cpu.set_s(0xff);     // Full stack
    cpu.set_p(0x30);     // Standard flags
    
    printf("Initial state:\n");
    printf("  PC: 0x%04x\n", cpu.get_pc());
    printf("  SP: 0x%02x\n", cpu.get_s());
    printf("  P: 0x%02x\n", cpu.get_p());
    printf("\n");
    
    // Prepare memory layout: JSR $1234 at $0800
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Ready line high
    bus_state |= BUS_BIT(BUS_SO_BIT);   // SO pin inactive (high)
    
    // Cycle 0: Opcode fetch (JSR = 0x20)
    printf("Cycle 0: Opcode fetch\n");
    bus_state = BUS_SET_DATA(bus_state, 0x20); // JSR opcode
    printf("  Bus data: 0x%02x (JSR)\n", BUS_GET_DATA(bus_state));
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x SP=0x%02x Cycle=%d\n", cpu.get_pc(), cpu.get_s(), cpu.get_cycle_step());
    
    // Cycle 1: Read address low byte ($34)
    printf("Cycle 1: Read address low\n");
    bus_state = BUS_SET_DATA(bus_state, 0x34); // Low byte of target address
    printf("  Address: 0x%04x, Bus data: 0x%02x\n", cpu.get_address(), BUS_GET_DATA(bus_state));
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x SP=0x%02x Cycle=%d\n", cpu.get_pc(), cpu.get_s(), cpu.get_cycle_step());
    
    // Cycle 2: Internal operation (stack pointer setup)
    printf("Cycle 2: Internal operation\n");
    printf("  Address: 0x%04x, RW: %s\n", cpu.get_address(), cpu.get_rw() ? "READ" : "WRITE");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x SP=0x%02x Cycle=%d\n", cpu.get_pc(), cpu.get_s(), cpu.get_cycle_step());
    
    // Cycle 3: Push PCH to stack
    printf("Cycle 3: Push PCH\n");
    printf("  Address: 0x%04x, RW: %s", cpu.get_address(), cpu.get_rw() ? "READ" : "WRITE");
    if (!cpu.get_rw()) {
        printf(", Write data: 0x%02x", cpu.get_write_data());
    }
    printf("\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x SP=0x%02x Cycle=%d\n", cpu.get_pc(), cpu.get_s(), cpu.get_cycle_step());
    
    // Cycle 4: Push PCL to stack  
    printf("Cycle 4: Push PCL\n");
    printf("  Address: 0x%04x, RW: %s", cpu.get_address(), cpu.get_rw() ? "READ" : "WRITE");
    if (!cpu.get_rw()) {
        printf(", Write data: 0x%02x", cpu.get_write_data());
    }
    printf("\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x SP=0x%02x Cycle=%d\n", cpu.get_pc(), cpu.get_s(), cpu.get_cycle_step());
    
    // Cycle 5: Read address high byte ($12)
    printf("Cycle 5: Read address high\n");
    bus_state = BUS_SET_DATA(bus_state, 0x12); // High byte of target address
    printf("  Address: 0x%04x, Bus data: 0x%02x\n", cpu.get_address(), BUS_GET_DATA(bus_state));
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x SP=0x%02x Cycle=%d\n", cpu.get_pc(), cpu.get_s(), cpu.get_cycle_step());
    
    // Cycle 6: Jump execution
    printf("Cycle 6: Jump execution\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x SP=0x%02x Cycle=%d\n", cpu.get_pc(), cpu.get_s(), cpu.get_cycle_step());
    
    printf("\nFinal state:\n");
    printf("  PC: 0x%04x (should be 0x1234)\n", cpu.get_pc());
    printf("  SP: 0x%02x (should be 0xfd - decremented by 2)\n", cpu.get_s());
    printf("  P: 0x%02x (should be preserved)\n", cpu.get_p());
    printf("\n");
    
    // Expected behavior:
    // - PC should be 0x1234 (target address)
    // - SP should be 0xfd (decremented from 0xff by 2)
    // - Stack should contain return address 0x0802 (PC+2 from original PC 0x0800)
    // - Stack[0x01ff] = 0x08 (PCH of return address)
    // - Stack[0x01fe] = 0x02 (PCL of return address)
    
    bool success = (cpu.get_pc() == 0x1234) && (cpu.get_s() == 0xfd);
    printf("JSR Test: %s\n", success ? "PASS" : "FAIL");
    
    if (!success) {
        printf("\nExpected:\n");
        printf("  PC: 0x1234, SP: 0xfd\n");
        printf("  Stack[0x01ff]: 0x08 (return address high)\n");
        printf("  Stack[0x01fe]: 0x02 (return address low)\n");
    }
    
    return 0;
}