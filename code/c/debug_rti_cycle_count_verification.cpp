#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "tests/fam65xx_cpp_test_harness.h"
#include <iostream>

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== RTI Cycle Count Architecture Analysis ===" << std::endl;
    
    // Create test CPU
    auto config = get_test_config<CpuVariant::NMOS_6502>();
    fam65xx<decltype(config)> cpu;
    
    // Initialize for test
    cpu.init_for_test();
    
    // Set up a simple RTI scenario
    cpu.set_pc(0x1000);
    cpu.set_sp(0xFD);  // Stack has return address and status
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_sp() << std::endl;
    
    // Create test harness with memory containing RTI instruction
    fam65xx_cpp_test_harness<decltype(config)> harness(cpu);
    
    // Set up memory: RTI at PC location
    harness.write_memory(0x1000, 0x40);  // RTI instruction
    
    // Set up stack with return address and status
    harness.write_memory(0x01FE, 0x30);  // Status register (P=0x30)
    harness.write_memory(0x01FF, 0x34);  // Return address low (0x1234)
    harness.write_memory(0x0100, 0x12);  // Return address high
    
    std::cout << "\nStack setup:" << std::endl;
    std::cout << "  [0x01FE] = 0x30 (Status)" << std::endl;
    std::cout << "  [0x01FF] = 0x34 (PC Low)" << std::endl;
    std::cout << "  [0x0100] = 0x12 (PC High)" << std::endl;
    
    // Count cycles during RTI execution
    int cycle_count = 0;
    uint8_t initial_step = cpu.get_cycle_step();
    uint16_t initial_opcode = cpu.get_opcode();
    
    std::cout << "\nExecuting RTI instruction:" << std::endl;
    std::cout << "  Initial step: " << (int)initial_step << std::endl;
    std::cout << "  Initial opcode: 0x" << std::hex << initial_opcode << std::endl;
    
    // Execute until instruction completes
    bool instruction_complete = false;
    while (!instruction_complete && cycle_count < 10) {
        uint8_t prev_step = cpu.get_cycle_step();
        uint16_t prev_opcode = cpu.get_opcode();
        
        // Execute one cycle
        harness.cycle_tick();
        cycle_count++;
        
        uint8_t curr_step = cpu.get_cycle_step();
        uint16_t curr_opcode = cpu.get_opcode();
        
        std::cout << "  Cycle " << cycle_count << ": ";
        std::cout << "step " << (int)prev_step << "→" << (int)curr_step;
        std::cout << ", opcode 0x" << std::hex << prev_opcode << "→0x" << curr_opcode;
        std::cout << ", PC=0x" << cpu.get_pc();
        std::cout << ", P=0x" << (int)cpu.get_p() << std::endl;
        
        // Check if instruction completed (step went from non-zero to 0)
        if (prev_step > 0 && curr_step == 0) {
            instruction_complete = true;
            std::cout << "  *** INSTRUCTION COMPLETE ***" << std::endl;
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  Total cycles: " << cycle_count << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_sp() << std::endl;
    
    // Analyze cycle table structure
    std::cout << "\n=== RTI Cycle Table Analysis ===" << std::endl;
    for (int step = 1; step <= 8; step++) {
        auto cycle = cpu.GET_CYCLE(0x40, step);
        MemOp mem_op = static_cast<MemOp>(cycle.mem_op);
        DataOp data_op = static_cast<DataOp>(cycle.data_op);
        bool has_sync = cycle.is_sync();
        
        std::cout << "  Step " << step << ": ";
        std::cout << "MemOp=" << (int)mem_op;
        std::cout << ", DataOp=" << (int)data_op;
        std::cout << ", SYNC=" << (has_sync ? "YES" : "NO");
        
        if (has_sync) {
            std::cout << " *** SYNC STEP ***";
        }
        std::cout << std::endl;
        
        // Stop at sync to avoid reading invalid cycles
        if (has_sync) break;
    }
    
    if (cycle_count == 6) {
        std::cout << "\n✓ RTI executed in correct 6 cycles" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ RTI executed in " << cycle_count << " cycles (should be 6)" << std::endl;
        std::cout << "ISSUE: Cycle counting architecture includes opcode fetch as separate cycle" << std::endl;
        return 1;
    }
}