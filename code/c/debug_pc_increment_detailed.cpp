#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <cstdio>

using namespace fam65xx_cpp;

int main() {
    printf("=== PC INCREMENT DETAILED DEBUG ===\n");
    
    // Use config_6502 for NMOS processor
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up a simple JMP indirect test
    cpu.set_pc(0x2000);
    
    // Create a simple memory simulation
    uint8_t memory[0x10000] = {0};
    memory[0x2000] = 0x6C;  // JMP indirect opcode
    memory[0x2001] = 0x00;  // Low byte of indirect address
    memory[0x2002] = 0x10;  // High byte of indirect address
    memory[0x1000] = 0x34;  // Target low byte
    memory[0x1001] = 0x12;  // Target high byte
    
    printf("Initial: PC=0x%04X, PCL=0x%02X, PCH=0x%02X, Step=%d\n", 
           cpu.get_pc(), cpu.get_reg(CpuReg::PCL), cpu.get_reg(CpuReg::PCH), cpu.get_cycle_step());
    
    // Execute cycles with detailed PC tracking
    for (int cycle = 0; cycle < 8; cycle++) {
        printf("\n=== CYCLE %d ===\n", cycle);
        
        // Show state BEFORE cycle execution
        uint16_t pc_before = cpu.get_pc();
        uint8_t pcl_before = cpu.get_reg(CpuReg::PCL);
        uint8_t pch_before = cpu.get_reg(CpuReg::PCH);
        uint8_t step_before = cpu.get_cycle_step();
        uint16_t address_before = cpu.get_address();
        
        printf("BEFORE: PC=0x%04X (PCL=0x%02X, PCH=0x%02X), Step=%d, Address=0x%04X\n", 
               pc_before, pcl_before, pch_before, step_before, address_before);
        
        // Get cycle information
        if (step_before > 0) {
            auto cycle_desc = cpu.GET_CYCLE(cpu.get_opcode(), step_before);
            printf("        Cycle Info: MemOp=%d, DataOp=%d, AluOp=%d, SYNC=%s\n",
                   static_cast<int>(cycle_desc.get_mem_op()),
                   static_cast<int>(cycle_desc.get_data_op()),
                   static_cast<int>(cycle_desc.get_alu_op()),
                   cycle_desc.is_sync() ? "YES" : "NO");
        }
        
        // Create bus state and provide memory data
        bus_state_t bus_state = 0;
        
        // CRITICAL FIX: Set RDY bit HIGH to indicate bus is ready
        // Without this, CPU enters infinite wait state due to RDY line being LOW
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        BUS_SET_ADDR(bus_state, address_before);
        
        if (cpu.get_rw()) {
            uint8_t data = memory[address_before];
            BUS_SET_DATA(bus_state, data);
            printf("        Providing data: 0x%02X from address 0x%04X\n", data, address_before);
        }
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Show state AFTER cycle execution
        uint16_t pc_after = cpu.get_pc();
        uint8_t pcl_after = cpu.get_reg(CpuReg::PCL);
        uint8_t pch_after = cpu.get_reg(CpuReg::PCH);
        uint8_t step_after = cpu.get_cycle_step();
        
        printf("AFTER:  PC=0x%04X (PCL=0x%02X, PCH=0x%02X), Step=%d\n", 
               pc_after, pcl_after, pch_after, step_after);
        
        // Check for PC changes
        if (pc_after != pc_before) {
            printf("        *** PC CHANGED: 0x%04X -> 0x%04X (increment: %s) ***\n", 
                   pc_before, pc_after, (pc_after == pc_before + 1) ? "YES" : "NO");
        } else {
            printf("        *** PC DID NOT CHANGE (expected increment?) ***\n");
        }
        
        // Check for step changes  
        if (step_after != step_before) {
            printf("        *** STEP CHANGED: %d -> %d ***\n", step_before, step_after);
        } else {
            printf("        *** STEP DID NOT CHANGE ***\n");
        }
        
        // Stop if instruction completed
        if (step_after == 0) {
            printf("*** INSTRUCTION COMPLETED ***\n");
            break;
        }
        
        // Stop if stuck (same PC and step)
        if (pc_after == pc_before && step_after == step_before && cycle > 0) {
            printf("*** STUCK - SAME PC AND STEP ***\n");
            break;
        }
    }
    
    printf("\nFinal PC: 0x%04X (expected: 0x1234)\n", cpu.get_pc());
    
    return 0;
}