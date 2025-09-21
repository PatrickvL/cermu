#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

// Test memory class
class TestMemory {
public:
    uint8_t memory[0x10000] = {0};
    
    TestMemory() {
        // Set up test program: LDA #$42, LDA #$55
        memory[0x1000] = 0xA9;  // LDA #imm
        memory[0x1001] = 0x42;  // operand
        memory[0x1002] = 0xA9;  // LDA #imm  
        memory[0x1003] = 0x55;  // operand
        memory[0x1004] = 0xEA;  // NOP
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    TestMemory mem;
    
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    
    std::cout << "=== CPU Cycle Step Debug for LDA #$42 ===" << std::endl;
    std::cout << "Initial state: PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc()
              << ", cycle_step=" << std::dec << (int)cpu.get_cycle_step() << std::endl;
    
    // Execute 10 cycles to see what happens
    for (int cycle = 1; cycle <= 10; cycle++) {
        uint16_t address = cpu.get_address();
        uint8_t data = mem.read(address);
        bool is_write = !cpu.get_rw();
        
        std::cout << "\n--- Cycle " << cycle << " (BEFORE) ---" << std::endl;
        std::cout << "  Address: 0x" << std::hex << std::setw(4) << std::setfill('0') << address;
        std::cout << ", Data: 0x" << std::setw(2) << (int)data;
        std::cout << ", R/W: " << (is_write ? "WRITE" : "READ") << std::endl;
        std::cout << "  PC: 0x" << std::setw(4) << cpu.get_pc();
        std::cout << ", Opcode: 0x" << std::setw(2) << cpu.get_opcode();
        std::cout << ", Step: " << std::dec << (int)cpu.get_cycle_step() << std::endl;
        
        // Create bus state
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY high
        
        // Execute one cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "--- Cycle " << cycle << " (AFTER) ---" << std::endl;
        std::cout << "  PC: 0x" << std::hex << std::setw(4) << cpu.get_pc();
        std::cout << ", Opcode: 0x" << std::setw(2) << cpu.get_opcode();
        std::cout << ", Step: " << std::dec << (int)cpu.get_cycle_step() << std::endl;
        std::cout << "  A: 0x" << std::hex << std::setw(2) << (int)cpu.get_a() << std::endl;
        
        // Stop if we complete the first instruction
        if (cycle >= 2 && cpu.get_cycle_step() == 0) {
            std::cout << "\n!!! INSTRUCTION COMPLETED in cycle " << cycle << " !!!" << std::endl;
            break;
        }
    }
    
    return 0;
}