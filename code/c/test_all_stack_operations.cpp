#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/core/system_lines.h"
#include <cstdio>

using namespace fam65xx_cpp;

void test_pha(fam65xx<config_6502>& cpu) {
    printf("=== Testing PHA (0x48) ===\n");
    
    // Reset CPU
    cpu.init_for_test();
    cpu.set_pc(0x5063);
    cpu.set_a(0x3e);
    cpu.set_s(0xa0);
    cpu.set_p(0xac); // V flag = 0 initially
    
    printf("Initial: PC=0x%04x A=0x%02x SP=0x%02x P=0x%02x V=%d\n", 
           cpu.get_pc(), cpu.get_a(), cpu.get_s(), cpu.get_p(), (cpu.get_p() & P_OVERFLOW) ? 1 : 0);
    
    // Execute PHA instruction
    bus_state_t bus_state = 0;
    
    // Cycle 0: Opcode fetch
    bus_state = BUS_SET_DATA(bus_state, 0x48); // PHA opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 1: Dummy read
    bus_state = BUS_SET_DATA(bus_state, 0xff); // Dummy data
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 2: Stack write
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("Final:   PC=0x%04x A=0x%02x SP=0x%02x P=0x%02x V=%d\n", 
           cpu.get_pc(), cpu.get_a(), cpu.get_s(), cpu.get_p(), (cpu.get_p() & P_OVERFLOW) ? 1 : 0);
    
    bool success = (cpu.get_pc() == 0x5064) && (cpu.get_s() == 0x9f) && ((cpu.get_p() & P_OVERFLOW) == 0);
    printf("PHA Test: %s\n\n", success ? "PASS" : "FAIL");
}

void test_php(fam65xx<config_6502>& cpu) {
    printf("=== Testing PHP (0x08) ===\n");
    
    // Reset CPU
    cpu.init_for_test();
    cpu.set_pc(0x5063);
    cpu.set_a(0x3e);
    cpu.set_s(0xa0);
    cpu.set_p(0xac); // Test value
    
    printf("Initial: PC=0x%04x P=0x%02x SP=0x%02x\n", 
           cpu.get_pc(), cpu.get_p(), cpu.get_s());
    
    // Execute PHP instruction
    bus_state_t bus_state = 0;
    
    // Cycle 0: Opcode fetch
    bus_state = BUS_SET_DATA(bus_state, 0x08); // PHP opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 1: Dummy read
    bus_state = BUS_SET_DATA(bus_state, 0xff); // Dummy data
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 2: Stack write
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("Final:   PC=0x%04x P=0x%02x SP=0x%02x\n", 
           cpu.get_pc(), cpu.get_p(), cpu.get_s());
    
    bool success = (cpu.get_pc() == 0x5064) && (cpu.get_s() == 0x9f) && (cpu.get_p() == 0xac);
    printf("PHP Test: %s\n\n", success ? "PASS" : "FAIL");
}

void test_pla(fam65xx<config_6502>& cpu) {
    printf("=== Testing PLA (0x68) ===\n");
    
    // Reset CPU
    cpu.init_for_test();
    cpu.set_pc(0x5063);
    cpu.set_a(0x00); // Different from stack value
    cpu.set_s(0x9f); // Stack pointer after a push
    cpu.set_p(0xac);
    
    printf("Initial: PC=0x%04x A=0x%02x SP=0x%02x P=0x%02x\n", 
           cpu.get_pc(), cpu.get_a(), cpu.get_s(), cpu.get_p());
    
    // Execute PLA instruction
    bus_state_t bus_state = 0;
    
    // Cycle 0: Opcode fetch
    bus_state = BUS_SET_DATA(bus_state, 0x68); // PLA opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 1: Dummy read
    bus_state = BUS_SET_DATA(bus_state, 0xff); // Dummy data
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 2: Stack read (increment SP)
    bus_state = BUS_SET_DATA(bus_state, 0x3e); // Value from stack
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 3: Load to A
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("Final:   PC=0x%04x A=0x%02x SP=0x%02x P=0x%02x\n", 
           cpu.get_pc(), cpu.get_a(), cpu.get_s(), cpu.get_p());
    
    bool success = (cpu.get_pc() == 0x5064) && (cpu.get_a() == 0x3e) && (cpu.get_s() == 0xa0);
    printf("PLA Test: %s\n\n", success ? "PASS" : "FAIL");
}

void test_plp(fam65xx<config_6502>& cpu) {
    printf("=== Testing PLP (0x28) ===\n");
    
    // Reset CPU
    cpu.init_for_test();
    cpu.set_pc(0x5063);
    cpu.set_s(0x9f); // Stack pointer after a push
    cpu.set_p(0x00); // Different from stack value
    
    printf("Initial: PC=0x%04x P=0x%02x SP=0x%02x\n", 
           cpu.get_pc(), cpu.get_p(), cpu.get_s());
    
    // Execute PLP instruction
    bus_state_t bus_state = 0;
    
    // Cycle 0: Opcode fetch
    bus_state = BUS_SET_DATA(bus_state, 0x28); // PLP opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 1: Dummy read
    bus_state = BUS_SET_DATA(bus_state, 0xff); // Dummy data
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 2: Stack read (increment SP)
    bus_state = BUS_SET_DATA(bus_state, 0xac); // Value from stack
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 3: Load to P
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    bus_state |= BUS_BIT(BUS_SO_BIT);
    bus_state = cpu.cycle_tick(bus_state);
    
    printf("Final:   PC=0x%04x P=0x%02x SP=0x%02x\n", 
           cpu.get_pc(), cpu.get_p(), cpu.get_s());
    
    bool success = (cpu.get_pc() == 0x5064) && (cpu.get_p() == 0xac) && (cpu.get_s() == 0xa0);
    printf("PLP Test: %s\n\n", success ? "PASS" : "FAIL");
}

int main() {
    printf("=== Complete Stack Operations Test ===\n");
    
    fam65xx<config_6502> cpu;
    
    test_pha(cpu);
    test_php(cpu);
    test_pla(cpu);
    test_plp(cpu);
    
    printf("All stack operation tests completed!\n");
    return 0;
}