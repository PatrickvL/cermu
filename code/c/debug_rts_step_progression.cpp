#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

class StepMemory {
private:
    uint8_t memory[0x10000];
    
public:
    StepMemory() {
        for (int i = 0; i < 0x10000; i++) {
            memory[i] = 0x00;
        }
        memory[0x4000] = 0x60;  // RTS instruction
        memory[0x01FD] = 0x55;  // Low byte
        memory[0x01FE] = 0x34;  // High byte
    }
    
    uint8_t read(uint16_t address) {
        return memory[address];
    }
};

int main() {
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    StepMemory memory;
    
    std::cout << "=== RTS Step Progression Debug ===" << std::endl;
    
    cpu.init_for_test();
    cpu.set_pc(0x4000);
    cpu.set_sp(0xFC);
    
    std::cout << "\nTracing step progression logic..." << std::endl;
    
    // Execute opcode fetch (step 0 → 1)
    std::cout << "\n--- OPCODE FETCH (Step 0 → 1) ---" << std::endl;
    uint16_t addr = cpu.get_address();
    uint8_t data = memory.read(addr);
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, data);
    
    std::cout << "Before cycle_tick(): Step=" << (int)cpu.get_cycle_step() 
              << ", PC=0x" << std::hex << cpu.get_pc() << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle_tick(): Step=" << (int)cpu.get_cycle_step() 
              << ", PC=0x" << std::hex << cpu.get_pc()
              << ", Opcode=0x" << std::hex << cpu.get_opcode() << std::endl;
    
    // Now execute step 1 and see if it advances to step 2
    std::cout << "\n--- STEP 1 EXECUTION ---" << std::endl;
    
    // Get cycle information for step 1
    auto cycle1 = fam65xx_cpp::fam65xx<Config>::GET_CYCLE(0x60, 1);
    std::cout << "Step 1 cycle info:" << std::endl;
    std::cout << "  MemOp: " << (int)cycle1.mem_op << std::endl;
    std::cout << "  DataOp: " << (int)cycle1.data_op << std::endl;
    std::cout << "  AluOp: " << (int)cycle1.alu_op << std::endl;
    std::cout << "  Sync: " << (cycle1.is_sync() ? "YES" : "NO") << std::endl;
    
    addr = cpu.get_address();
    data = memory.read(addr);
    bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, data);
    
    std::cout << "Before step 1 cycle_tick(): Step=" << (int)cpu.get_cycle_step() 
              << ", Address=0x" << std::hex << addr << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After step 1 cycle_tick(): Step=" << (int)cpu.get_cycle_step() 
              << ", PC=0x" << std::hex << cpu.get_pc() << std::endl;
    
    // Check if we advanced to step 2
    if (cpu.get_cycle_step() == 2) {
        std::cout << "SUCCESS: Advanced to step 2!" << std::endl;
        
        // Test step 2
        std::cout << "\n--- STEP 2 EXECUTION ---" << std::endl;
        auto cycle2 = fam65xx_cpp::fam65xx<Config>::GET_CYCLE(0x60, 2);
        std::cout << "Step 2 cycle info:" << std::endl;
        std::cout << "  MemOp: " << (int)cycle2.mem_op << std::endl;
        std::cout << "  DataOp: " << (int)cycle2.data_op << std::endl;
        std::cout << "  AluOp: " << (int)cycle2.alu_op << std::endl;
        std::cout << "  Sync: " << (cycle2.is_sync() ? "YES" : "NO") << std::endl;
        
    } else if (cpu.get_cycle_step() == 1) {
        std::cout << "FAILED: Still stuck at step 1!" << std::endl;
        
        // Analyze why step didn't advance
        std::cout << "\nAnalyzing why step 1 didn't advance..." << std::endl;
        std::cout << "Step 1 sync flag: " << (cycle1.is_sync() ? "YES" : "NO") << std::endl;
        std::cout << "Expected: Step should advance because sync=NO" << std::endl;
        
    } else if (cpu.get_cycle_step() == 0) {
        std::cout << "ERROR: Instruction completed after step 1 (should not happen!)" << std::endl;
    } else {
        std::cout << "UNEXPECTED: Step = " << (int)cpu.get_cycle_step() << std::endl;
    }
    
    return 0;
}