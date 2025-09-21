#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

int main() {
    std::cout << "=== Testing Simple LDA #$42 (0xA9) Instruction ===" << std::endl;
    
    TestCPU cpu;
    cpu.init_for_test();
    
    // Set up test case: LDA #$42
    cpu.set_pc(0x1000);
    
    // Create memory with the instruction
    std::array<uint8_t, 65536> memory = {};
    memory[0x1000] = 0xA9;  // LDA #imm opcode
    memory[0x1001] = 0x42;  // Immediate value
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "CPU A = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)cpu.get_a() << std::endl;
    std::cout << "CPU P = 0x" << std::setw(2) << (int)cpu.get_p() << std::endl;
    
    // Execute instruction cycle by cycle
    bus_state_t bus_state = 0;
    int cycles = 0;
    
    std::cout << "\nExecution trace:" << std::endl;
    do {
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        std::cout << "Cycle " << std::dec << cycles + 1 << ": ";
        std::cout << "Addr=0x" << std::hex << std::setw(4) << addr;
        std::cout << " " << (is_read ? "READ" : "WRITE");
        
        if (is_read) {
            // Read operation
            uint8_t data = memory[addr];
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << " data=0x" << std::setw(2) << (int)data;
        } else {
            // Write operation  
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << " data=0x" << std::setw(2) << (int)data;
        }
        
        std::cout << " (step=" << std::dec << (int)cpu.get_cycle_step() << ")";
        std::cout << std::endl;
        
        bus_state = cpu.cycle_tick(bus_state);
        cycles++;
    } while (cpu.get_cycle_step() != 0 && cycles < 10);
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "CPU A = 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)cpu.get_a() << std::endl;
    std::cout << "CPU P = 0x" << std::setw(2) << (int)cpu.get_p() << std::endl;
    std::cout << "PC = 0x" << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "Cycles executed: " << std::dec << cycles << std::endl;
    
    // Expected results for LDA #$42:
    // - A register should be 0x42
    // - P register should have N=0, Z=0 (0x42 is positive and non-zero)
    // - PC should be 0x1002 (instruction is 2 bytes)
    
    bool a_correct = (cpu.get_a() == 0x42);
    bool pc_correct = (cpu.get_pc() == 0x1002);
    
    std::cout << "\nValidation:" << std::endl;
    std::cout << "A result: " << (a_correct ? "PASS" : "FAIL") << " (expected 0x42, got 0x" << std::hex << std::setw(2) << (int)cpu.get_a() << ")" << std::endl;
    std::cout << "PC result: " << (pc_correct ? "PASS" : "FAIL") << " (expected 0x1002, got 0x" << std::setw(4) << cpu.get_pc() << ")" << std::endl;
    
    bool all_pass = a_correct && pc_correct;
    std::cout << "\nOverall: " << (all_pass ? "✅ PASS" : "❌ FAIL") << std::endl;
    
    return all_pass ? 0 : 1;
}