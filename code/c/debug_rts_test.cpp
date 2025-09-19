#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/core/system_lines.h"
#include <cstdio>

using namespace fam65xx_cpp;

int main() {
    printf("=== RTS Instruction Debugging ===\n");
    
    // Create CPU and initialize for testing
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up test state for RTS
    cpu.set_pc(0x2000);  // RTS instruction location
    cpu.set_s(0xFD);     // Stack pointer (will be incremented twice during RTS)
    cpu.set_p(0x30);     // Standard flags
    
    printf("Initial state:\n");
    printf("  PC: 0x%04x, SP: 0x%02x, P: 0x%02x\n", cpu.get_pc(), cpu.get_s(), cpu.get_p());
    printf("  ABL: 0x%02x, ABH: 0x%02x\n", cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    printf("\n");
    
    // Set up stack with return address 0x1234
    // RTS expects: PCL at (SP+1), PCH at (SP+2)
    // SP=0xFD, so PCL at 0x01FE, PCH at 0x01FF
    cpu.write_memory(0x01FE, 0x34);  // PCL (low byte)
    cpu.write_memory(0x01FF, 0x12);  // PCH (high byte)
    
    printf("Stack setup:\n");
    printf("  Stack[0x01FE] = 0x%02x (PCL)\n", cpu.read_memory(0x01FE));
    printf("  Stack[0x01FF] = 0x%02x (PCH)\n", cpu.read_memory(0x01FF));
    printf("\n");
    
    // Prepare bus state
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Ready line high
    bus_state |= BUS_BIT(BUS_SO_BIT);   // SO pin inactive (high)
    
    // Cycle 0: Opcode fetch (RTS = 0x60)
    printf("Cycle 0: Opcode fetch\n");
    bus_state = BUS_SET_DATA(bus_state, 0x60); // RTS opcode
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, SP=0x%02x, ABL=0x%02x, ABH=0x%02x\n",
           cpu.get_pc(), cpu.get_s(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 1: Read PC_INC (dummy read)
    printf("Cycle 1: Dummy read PC+1\n");
    bus_state = BUS_SET_DATA(bus_state, 0x00); // Dummy data
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, SP=0x%02x, ABL=0x%02x, ABH=0x%02x\n",
           cpu.get_pc(), cpu.get_s(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 2: Internal operation (read SP)
    printf("Cycle 2: Internal operation\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, SP=0x%02x, ABL=0x%02x, ABH=0x%02x\n",
           cpu.get_pc(), cpu.get_s(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 3: Pull PCL from stack (SP+1)
    printf("Cycle 3: Pull PCL from stack\n");
    bus_state = BUS_SET_DATA(bus_state, 0x34); // PCL data from stack
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, SP=0x%02x, ABL=0x%02x, ABH=0x%02x\n",
           cpu.get_pc(), cpu.get_s(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 4: Pull PCH from stack (SP+1 again, now SP+2)
    printf("Cycle 4: Pull PCH from stack\n");
    bus_state = BUS_SET_DATA(bus_state, 0x12); // PCH data from stack
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, SP=0x%02x, ABL=0x%02x, ABH=0x%02x\n",
           cpu.get_pc(), cpu.get_s(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 5: Internal - increment PC
    printf("Cycle 5: Internal - increment PC\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, SP=0x%02x, ABL=0x%02x, ABH=0x%02x\n",
           cpu.get_pc(), cpu.get_s(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    // Cycle 6: SYNC - instruction complete
    printf("Cycle 6: SYNC - instruction complete\n");
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After: PC=0x%04x, SP=0x%02x, ABL=0x%02x, ABH=0x%02x\n",
           cpu.get_pc(), cpu.get_s(), cpu.get_reg(CpuReg::ABL), cpu.get_reg(CpuReg::ABH));
    
    printf("\nFinal state:\n");
    printf("  PC: 0x%04x (should be 0x1235 = 0x1234 + 1)\n", cpu.get_pc());
    printf("  SP: 0x%02x (should be 0xFF = 0xFD + 2)\n", cpu.get_s());
    
    bool pc_correct = (cpu.get_pc() == 0x1235);
    bool sp_correct = (cpu.get_s() == 0xFF);
    bool success = pc_correct && sp_correct;
    
    printf("RTS Test Results:\n");
    printf("  PC correct: %s\n", pc_correct ? "YES" : "NO");
    printf("  SP correct: %s\n", sp_correct ? "YES" : "NO");
    printf("  Overall: %s\n", success ? "PASS" : "FAIL");
    
    return 0;
}