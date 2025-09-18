#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/core/system_lines.h"
#include <cstdio>

using namespace fam65xx_cpp;

int main() {
    printf("=== PHA V Flag Detailed Debug Test ===\n");
    
    // Create CPU and initialize for testing
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up test state
    cpu.set_pc(0x5063);
    cpu.set_a(0x3e);
    cpu.set_s(0xa0);
    cpu.set_p(0xac); // V flag = 0 initially
    
    printf("Initial state:\n");
    printf("  PC: 0x%04x\n", cpu.get_pc());
    printf("  A: 0x%02x\n", cpu.get_a());
    printf("  SP: 0x%02x\n", cpu.get_s());
    printf("  P: 0x%02x\n", cpu.get_p());
    printf("  V flag: %d\n", (cpu.get_p() & P_OVERFLOW) ? 1 : 0);
    printf("\n");
    
    // Set up bus state with proper pin states
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0x48); // PHA opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Ready line high
    bus_state |= BUS_BIT(BUS_SO_BIT);   // SO pin inactive (high)
    
    // Execute opcode fetch (cycle 0)
    printf("Executing opcode fetch (cycle 0)...\n");
    printf("  Bus data: 0x%02x (PHA)\n", BUS_GET_DATA(bus_state));
    printf("  SO pin state: %s\n", (bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH (inactive)" : "LOW (active)");
    
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("After opcode fetch:\n");
    printf("  PC: 0x%04x\n", cpu.get_pc());
    printf("  P: 0x%02x\n", cpu.get_p());
    printf("  V flag: %d\n", (cpu.get_p() & P_OVERFLOW) ? 1 : 0);
    printf("  Cycle step: %d\n", cpu.get_cycle_step());
    printf("  Opcode: 0x%02x\n", cpu.get_opcode());
    printf("\n");
    
    // Prepare for cycle 1 (dummy read)
    bus_state = BUS_SET_DATA(bus_state, 0xff); // Dummy data for PC read
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Ready line high
    bus_state |= BUS_BIT(BUS_SO_BIT);   // SO pin inactive (high)
    
    printf("Executing cycle 1 (dummy read)...\n");
    printf("  Address: 0x%04x\n", cpu.get_address());
    printf("  SO pin state: %s\n", (bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH (inactive)" : "LOW (active)");
    printf("  State flags before: 0x%08x\n", cpu.get_state_flags());
    
    // Check if SO edge is set before execution
    printf("  SO edge state before: %s\n", (cpu.get_state_flags() & STATE_SO_EDGE) ? "SET" : "CLEAR");
    
    // Execute cycle 1
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("After cycle 1:\n");
    printf("  PC: 0x%04x\n", cpu.get_pc());
    printf("  P: 0x%02x\n", cpu.get_p());
    printf("  V flag: %d <-- CHECK FOR V FLAG CHANGE\n", (cpu.get_p() & P_OVERFLOW) ? 1 : 0);
    printf("  Cycle step: %d\n", cpu.get_cycle_step());
    printf("  State flags after: 0x%08x\n", cpu.get_state_flags());
    printf("  SO edge state after: %s\n", (cpu.get_state_flags() & STATE_SO_EDGE) ? "SET" : "CLEAR");
    printf("\n");
    
    // Prepare for cycle 2 (stack write)
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Ready line high
    bus_state |= BUS_BIT(BUS_SO_BIT);   // SO pin inactive (high)
    
    printf("Executing cycle 2 (stack write)...\n");
    printf("  Address: 0x%04x\n", cpu.get_address());
    printf("  Is write: %s\n", cpu.get_rw() ? "NO" : "YES");
    printf("  Write data: 0x%02x\n", cpu.get_write_data());
    printf("  SO pin state: %s\n", (bus_state & BUS_BIT(BUS_SO_BIT)) ? "HIGH (inactive)" : "LOW (active)");
    
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("After cycle 2 (FINAL):\n");
    printf("  PC: 0x%04x\n", cpu.get_pc());
    printf("  SP: 0x%02x\n", cpu.get_s());
    printf("  P: 0x%02x\n", cpu.get_p());
    printf("  V flag: %d <-- FINAL V FLAG STATE\n", (cpu.get_p() & P_OVERFLOW) ? 1 : 0);
    printf("  Cycle step: %d\n", cpu.get_cycle_step());
    printf("  State flags final: 0x%08x\n", cpu.get_state_flags());
    printf("\n");
    
    printf("Expected:\n");
    printf("  PC: 0x5064\n");
    printf("  P: 0xac (V=0)\n");
    printf("  Stack[0x19f]: 0x3e\n");
    
    // Test if V flag corruption is related to processor status register itself
    printf("\nProcessor status register analysis:\n");
    printf("  Expected P: 0xac = 10101100 binary\n");
    printf("  Actual P:   0x%02x = ", cpu.get_p());
    for (int i = 7; i >= 0; i--) {
        printf("%d", (cpu.get_p() >> i) & 1);
    }
    printf(" binary\n");
    printf("  V flag bit (bit 6): %s\n", (cpu.get_p() & P_OVERFLOW) ? "SET" : "CLEAR");
    
    return 0;
}