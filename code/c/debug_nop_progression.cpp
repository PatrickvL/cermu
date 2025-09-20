#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

class SimpleMemory {
private:
    uint8_t memory[0x10000];
    
public:
    SimpleMemory() {
        for (int i = 0; i < 0x10000; i++) {
            memory[i] = 0x00;
        }
        memory[0x4000] = 0xEA;  // NOP instruction
    }
    
    uint8_t read(uint16_t address) {
        return memory[address];
    }
};

int main() {
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    SimpleMemory memory;
    
    std::cout << "=== NOP Step Progression Debug ===" << std::endl;
    
    cpu.init_for_test();
    cpu.set_pc(0x4000);
    cpu.set_sp(0xFC);
    
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
    
    // NOP should be a 1-cycle instruction that completes immediately
    std::cout << "\n--- STEP 1 EXECUTION (should complete) ---" << std::endl;
    
    // Get cycle information for step 1
    auto cycle1 = fam65xx_cpp::fam65xx<Config>::GET_CYCLE(0xEA, 1);
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
    
    // Check results
    if (cpu.get_cycle_step() == 0) {
        std::cout << "SUCCESS: NOP completed (step reset to 0)!" << std::endl;
    } else {
        std::cout << "FAILED: NOP didn't complete, step = " << (int)cpu.get_cycle_step() << std::endl;
    }
    
    return 0;
}