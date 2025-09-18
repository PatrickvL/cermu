#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/core/system_lines.h"
#include <cstdio>

using namespace fam65xx_cpp;

int main() {
    printf("=== JSR Register Debug Test ===\n");
    
    // Create CPU and initialize for testing
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up test state for JSR $1234
    cpu.set_pc(0x0800);  // Start address
    cpu.set_s(0xff);     // Full stack
    cpu.set_p(0x30);     // Standard flags
    
    printf("Initial state:\n");
    printf("  PC: 0x%04x, SP: 0x%02x, P: 0x%02x\n", cpu.get_pc(), cpu.get_s(), cpu.get_p());
    printf("  ABL: 0x%02x, ABH: 0x%02x\n", cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    printf("\n");
    
    // Prepare memory layout: JSR $1234 at $0800
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Ready line high
    bus_state |= BUS_BIT(BUS_SO_BIT);   // SO pin inactive (high)
    
    // Cycle 0: Opcode fetch (JSR = 0x20)
    printf("Cycle 0: Opcode fetch\n");
    bus_state = BUS_SET_DATA(bus_state, 0x20); // JSR opcode
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, ABL=0x%02x, ABH=0x%02x\n", 
           cpu.get_pc(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 1: Read address low byte ($34)
    printf("Cycle 1: Read address low\n");
    bus_state = BUS_SET_DATA(bus_state, 0x34); // Low byte of target address
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, ABL=0x%02x, ABH=0x%02x\n", 
           cpu.get_pc(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 2: Internal operation (stack pointer setup)
    printf("Cycle 2: Internal operation\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, ABL=0x%02x, ABH=0x%02x\n", 
           cpu.get_pc(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 3: Push PCH to stack
    printf("Cycle 3: Push PCH\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, ABL=0x%02x, ABH=0x%02x\n", 
           cpu.get_pc(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 4: Push PCL to stack  
    printf("Cycle 4: Push PCL\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, ABL=0x%02x, ABH=0x%02x\n", 
           cpu.get_pc(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 5: Read address high byte ($12)
    printf("Cycle 5: Read address high\n");
    bus_state = BUS_SET_DATA(bus_state, 0x12); // High byte of target address
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, ABL=0x%02x, ABH=0x%02x\n", 
           cpu.get_pc(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 6: Jump execution
    printf("Cycle 6: Jump execution\n");
    printf("  Before jump: ABL=0x%02x, ABH=0x%02x (should be 0x34, 0x12)\n", 
           cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, ABL=0x%02x, ABH=0x%02x\n", 
           cpu.get_pc(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    printf("\nFinal state:\n");
    printf("  PC: 0x%04x (should be 0x1234)\n", cpu.get_pc());
    printf("  Expected: ABL=0x34, ABH=0x12 → PC=0x1234\n");
    
    bool success = (cpu.get_pc() == 0x1234);
    printf("JSR Register Test: %s\n", success ? "PASS" : "FAIL");
    
    return 0;
}