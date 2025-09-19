#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/core/system_lines.h"
#include <cstdio>

using namespace fam65xx_cpp;

int main() {
    printf("=== RTS Cycle Execution Framework Debug ===\n");
    
    // Create CPU and initialize for testing
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up test state for RTS
    cpu.set_pc(0x2000);  // RTS instruction location
    cpu.set_s(0xFD);     // Stack pointer
    cpu.set_p(0x30);     // Standard flags
    
    printf("Initial state:\n");
    printf("  PC: 0x%04x, SP: 0x%02x, Opcode: 0x%04x, Cycle: %d\n", 
           cpu.get_pc(), cpu.get_s(), cpu.get_opcode(), cpu.get_cycle_step());
    
    // Prepare bus state
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Ready line high
    bus_state |= BUS_BIT(BUS_SO_BIT);   // SO pin inactive (high)
    
    printf("\n=== CYCLE 0: OPCODE FETCH ===\n");
    bus_state = BUS_SET_DATA(bus_state, 0x60); // RTS opcode
    printf("Before cycle 0: opcode=0x%04x, cycle_step=%d\n", cpu.get_opcode(), cpu.get_cycle_step());
    bus_state = cpu.cycle_tick(bus_state);
    printf("After cycle 0:  opcode=0x%04x, cycle_step=%d\n", cpu.get_opcode(), cpu.get_cycle_step());
    
    // Check cycle table for RTS instruction
    printf("\n=== RTS CYCLE TABLE VERIFICATION ===\n");
    for (int cycle = 1; cycle <= 6; cycle++) {
        const fam65xx_cpp::cycle_desc_t cycle_desc = cpu.GET_CYCLE(0x60, cycle);
        printf("Cycle %d: MemOp=%d, DataOp=%d, AluOp=%d, Sync=%s\n",
               cycle,
               static_cast<int>(cycle_desc.mem_op),
               static_cast<int>(cycle_desc.data_op),
               static_cast<int>(cycle_desc.alu_op),
               cycle_desc.is_sync() ? "YES" : "NO");
    }
    
    printf("\n=== ATTEMPTING CYCLE EXECUTION ===\n");
    
    // Try to execute cycles 1-6 manually
    for (int cycle = 1; cycle <= 6; cycle++) {
        printf("\n--- Attempting Cycle %d ---\n", cycle);
        printf("Before: opcode=0x%04x, cycle_step=%d\n", cpu.get_opcode(), cpu.get_cycle_step());
        
        // Set appropriate bus data for the cycle
        switch (cycle) {
            case 1:
                bus_state = BUS_SET_DATA(bus_state, 0x00); // Dummy data for PC+1 read
                break;
            case 3:
                bus_state = BUS_SET_DATA(bus_state, 0x34); // PCL from stack
                break;
            case 4:
                bus_state = BUS_SET_DATA(bus_state, 0x12); // PCH from stack
                break;
            default:
                bus_state = BUS_SET_DATA(bus_state, 0x00); // Default dummy data
                break;
        }
        
        // Execute the cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        printf("After:  opcode=0x%04x, cycle_step=%d\n", cpu.get_opcode(), cpu.get_cycle_step());
        printf("        PC=0x%04x, SP=0x%02x\n", cpu.get_pc(), cpu.get_s());
        
        // Check if the instruction completed unexpectedly
        if (cpu.get_cycle_step() == 0) {
            printf("*** INSTRUCTION COMPLETED EARLY! Expected cycle %d but got cycle_step=0 ***\n", cycle);
            break;
        }
    }
    
    printf("\n=== CYCLE COMPLETION ANALYSIS ===\n");
    printf("Final cycle_step: %d\n", cpu.get_cycle_step());
    printf("Final PC: 0x%04x\n", cpu.get_pc());
    printf("Final SP: 0x%02x\n", cpu.get_s());
    
    // Check instruction completion logic
    if (cpu.get_cycle_step() == 0) {
        printf("Instruction has completed.\n");
    } else {
        printf("Instruction is still in progress.\n");
    }
    
    // Compare with JSR instruction (which works)
    printf("\n=== JSR CYCLE TABLE COMPARISON ===\n");
    printf("JSR Cycles (0x20):\n");
    for (int cycle = 1; cycle <= 6; cycle++) {
        const fam65xx_cpp::cycle_desc_t cycle_desc = cpu.GET_CYCLE(0x20, cycle);
        printf("Cycle %d: MemOp=%d, DataOp=%d, AluOp=%d, Sync=%s\n",
               cycle,
               static_cast<int>(cycle_desc.mem_op),
               static_cast<int>(cycle_desc.data_op),
               static_cast<int>(cycle_desc.alu_op),
               cycle_desc.is_sync() ? "YES" : "NO");
    }
    
    return 0;
}