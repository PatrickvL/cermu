#include <iostream>
#include <cstdio>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

// Simple memory interface for debugging
class DebugMemory {
private:
    uint8_t memory[65536];
    
public:
    DebugMemory() {
        // Initialize all memory to 0
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
    }
    
    uint8_t read(uint16_t addr) const {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    std::cout << "=== RTI Single Case Debug ===\n";
    
    // Create CPU instance
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Create memory
    DebugMemory memory;
    
    // Set up a simple RTI test case
    // Place RTI instruction at 0x8000
    memory.write(0x8000, 0x40);  // RTI opcode
    
    // Set up stack with test values
    cpu.set_s(0xFC);  // Stack pointer (will be incremented during RTI)
    memory.write(0x01FD, 0x30);  // Status register on stack
    memory.write(0x01FE, 0x34);  // PC low byte on stack  
    memory.write(0x01FF, 0x12);  // PC high byte on stack
    
    // Set initial state
    cpu.set_pc(0x8000);
    cpu.set_p(0x24);  // Some initial flags
    
    std::cout << "Initial state:\n";
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_s() << std::endl;
    std::cout << "  P:  0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "  Stack[0x01FD]: 0x" << std::hex << (int)memory.read(0x01FD) << " (status)" << std::endl;
    std::cout << "  Stack[0x01FE]: 0x" << std::hex << (int)memory.read(0x01FE) << " (PC low)" << std::endl;
    std::cout << "  Stack[0x01FF]: 0x" << std::hex << (int)memory.read(0x01FF) << " (PC high)" << std::endl;
    
    // Execute RTI instruction cycle by cycle
    std::cout << "\nExecuting RTI instruction:\n";
    
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        uint8_t write_data = is_write ? cpu.get_write_data() : 0;
        
        std::cout << "Cycle " << cycle << ": addr=0x" << std::hex << addr 
                  << " " << (is_write ? "WRITE" : "READ") 
                  << " opcode=0x" << std::hex << cpu.get_opcode()
                  << " step=" << std::dec << (int)cpu.get_cycle_step();
        
        if (is_write) {
            std::cout << " data=0x" << std::hex << (int)write_data;
            memory.write(addr, write_data);
        }
        std::cout << std::endl;
        
        // Create bus state with memory data
        bus_state_t bus_state = 0;
        if (!is_write) {
            uint8_t read_data = memory.read(addr);
            bus_state = BUS_SET_DATA(bus_state, read_data);
            std::cout << "  Read data: 0x" << std::hex << (int)read_data << std::endl;
        }
        
        // Set RDY line high (ready)
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0) {
            std::cout << "  *** Instruction complete ***\n";
            break;
        }
        
        // Show updated state after each cycle
        std::cout << "  After cycle: PC=0x" << std::hex << cpu.get_pc() 
                  << " SP=0x" << std::hex << (int)cpu.get_s()
                  << " P=0x" << std::hex << (int)cpu.get_p() << std::endl;
    }
    
    std::cout << "\nFinal state:\n";
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_s() << std::endl;
    std::cout << "  P:  0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    std::cout << "\nExpected final state:\n";
    std::cout << "  PC: 0x1234 (from stack: 0x12 high, 0x34 low)\n";
    std::cout << "  SP: 0xFF (incremented 3 times from 0xFC)\n";
    std::cout << "  P:  0x30 (pulled from stack)\n";
    
    // Check results
    bool pc_correct = (cpu.get_pc() == 0x1234);
    bool sp_correct = (cpu.get_s() == 0xFF);
    bool p_correct = (cpu.get_p() == 0x30);
    
    std::cout << "\nResults:\n";
    std::cout << "  PC: " << (pc_correct ? "✓ CORRECT" : "✗ WRONG") << std::endl;
    std::cout << "  SP: " << (sp_correct ? "✓ CORRECT" : "✗ WRONG") << std::endl;
    std::cout << "  P:  " << (p_correct ? "✓ CORRECT" : "✗ WRONG") << std::endl;
    
    if (pc_correct && sp_correct && p_correct) {
        std::cout << "\n🎉 RTI INSTRUCTION WORKING CORRECTLY!\n";
    } else {
        std::cout << "\n❌ RTI INSTRUCTION HAS ISSUES\n";
    }
    
    return 0;
}