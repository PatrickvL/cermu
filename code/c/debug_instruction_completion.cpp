#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

// Test harness that mimics ProcessorTests but focuses on instruction completion logic
class DebugMemory {
private:
    uint8_t memory[65536];
    
public:
    DebugMemory() {
        // Initialize memory
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0x00;
        }
        
        // Set up test program: NOP at 0x4000
        memory[0x4000] = 0xEA; // NOP instruction
        memory[0x4001] = 0x00; // Dummy data
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    std::cout << "=== Instruction Completion Logic Debug ===" << std::endl << std::endl;
    
    // Create CPU and memory
    using Config = config_6502;
    fam65xx<Config> cpu;
    DebugMemory memory;
    
    // Initialize CPU for testing
    cpu.init_for_test();
    cpu.set_pc(0x4000);  // Start at NOP instruction
    
    std::cout << "--- OPCODE FETCH (Step 0 → 1) ---" << std::endl;
    std::cout << "Before cycle_tick(): Step=" << (int)cpu.get_cycle_step()
              << ", PC=0x" << std::hex << cpu.get_pc() << std::endl;
    
    // Cycle 0: Opcode fetch
    {
        uint16_t addr = cpu.get_address();
        bool rw = cpu.get_rw();
        uint8_t data = memory.read(addr);
        
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // Set RDY line high (ready)
        
        bus_state = cpu.cycle_tick(bus_state);
    }
    
    std::cout << "After cycle_tick(): Step=" << (int)cpu.get_cycle_step()
              << ", PC=0x" << std::hex << cpu.get_pc()
              << ", Opcode=0x" << std::hex << cpu.get_opcode() << std::endl;
    
    std::cout << std::endl << "--- STEP 1 EXECUTION (should complete) ---" << std::endl;
    
    // Get cycle information BEFORE execution
    const auto cycle = cpu.GET_CYCLE(cpu.get_opcode(), cpu.get_cycle_step());
    std::cout << "Step 1 cycle info:" << std::endl;
    std::cout << "  MemOp: " << (int)cycle.mem_op << std::endl;
    std::cout << "  DataOp: " << std::hex << (int)cycle.data_op << std::endl;
    std::cout << "  AluOp: " << (int)cycle.alu_op << std::endl;
    std::cout << "  Sync: " << (cycle.is_sync() ? "YES" : "NO") << std::endl;
    
    std::cout << "Before step 1 cycle_tick(): Step=" << (int)cpu.get_cycle_step()
              << ", Address=0x" << std::hex << cpu.get_address() << std::endl;
    
    // **CRITICAL DEBUG**: Check instruction completion logic step by step
    std::cout << std::endl << "--- INSTRUCTION COMPLETION LOGIC DEBUG ---" << std::endl;
    std::cout << "cycle.is_sync() returns: " << (cycle.is_sync() ? "true" : "false") << std::endl;
    
    // Check if this is a branch instruction (should be false for NOP)
    uint8_t opcode = cpu.get_opcode();
    bool is_branch = (opcode >= 0x10 && opcode <= 0xF0 && (opcode & 0x1F) == 0x10);
    std::cout << "Is branch instruction: " << (is_branch ? "true" : "false") << std::endl;
    
    // Cycle 1: NOP execution
    {
        uint16_t addr = cpu.get_address();
        bool rw = cpu.get_rw();
        uint8_t data = memory.read(addr);
        
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // Set RDY line high (ready)
        
        std::cout << "About to call cycle_tick() with instruction_complete expected to be: "
                  << (cycle.is_sync() ? "true" : "false") << std::endl;
        
        bus_state = cpu.cycle_tick(bus_state);
    }
    
    std::cout << "After step 1 cycle_tick(): Step=" << (int)cpu.get_cycle_step()
              << ", PC=0x" << std::hex << cpu.get_pc() << std::endl;
    
    // Check final result
    if (cpu.get_cycle_step() == 0) {
        std::cout << "SUCCESS: NOP completed correctly, step = 0" << std::endl;
    } else {
        std::cout << "FAILED: NOP didn't complete, step = " << (int)cpu.get_cycle_step() << std::endl;
        
        // Additional debugging: Try one more cycle to see what happens
        std::cout << std::endl << "--- TRYING ONE MORE CYCLE ---" << std::endl;
        std::cout << "Current cycle info for step " << (int)cpu.get_cycle_step() << ":" << std::endl;
        
        if (cpu.get_cycle_step() > 0) {
            const auto next_cycle = fam65xx<Config>::GET_CYCLE(cpu.get_opcode(), cpu.get_cycle_step());
            std::cout << "  MemOp: " << (int)next_cycle.mem_op << std::endl;
            std::cout << "  DataOp: " << std::hex << (int)next_cycle.data_op << std::endl;
            std::cout << "  AluOp: " << (int)next_cycle.alu_op << std::endl;
            std::cout << "  Sync: " << (next_cycle.is_sync() ? "YES" : "NO") << std::endl;
        }
    }
    
    return 0;
}