#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

// Create a simple test memory simulator
class TestMemory {
private:
    uint8_t memory[0x10000];
    
public:
    TestMemory() {
        // Initialize memory to 0
        for (int i = 0; i < 0x10000; i++) {
            memory[i] = 0x00;
        }
        
        // Set up test program for BPL (Branch if Plus)
        memory[0x1000] = 0x10;  // BPL opcode
        memory[0x1001] = 0x02;  // Branch offset +2
        memory[0x1002] = 0xEA;  // NOP (not taken path)
        memory[0x1003] = 0xEA;  // NOP (not taken path)
        memory[0x1004] = 0xEA;  // NOP (branch target)
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    std::cout << "=== BRANCH STATE DEBUG TEST ===" << std::endl;
    
    // Create CPU with 6502 configuration
    using CpuConfig = cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, '\x10', false, false, false>;
    fam65xx<CpuConfig> cpu;
    TestMemory mem;
    
    // Initialize CPU for testing
    cpu.init_for_test();
    
    // Set up initial state for BPL that should branch (N=0)
    cpu.set_pc(0x1000);   // Set PC to test program
    cpu.set_p(0x00);      // Clear all flags (N=0, so BPL should branch)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  P=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p();
    std::cout << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.get_pc();
    std::cout << " Flags=0x" << std::hex << std::setw(8) << std::setfill('0') << (int)cpu.get_state_flags() << std::dec << std::endl;
    
    // Set up memory with the test instruction
    mem.write(0x1000, 0x10);  // BPL opcode
    mem.write(0x1001, 0x02);  // Branch offset
    
    // Simulate instruction execution
    bus_state_t bus_state = 0;
    
    std::cout << "\nExecuting branch instruction:" << std::endl;
    
    // Execute several cycles to see what happens
    for (int cycle = 0; cycle < 5; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        if (!is_write) {
            uint8_t read_data = mem.read(addr);
            std::cout << "Cycle " << cycle << ": READ  addr=0x" 
                      << std::hex << std::setw(4) << std::setfill('0') << addr
                      << " data=0x" << std::setw(2) << (int)read_data 
                      << " opcode=0x" << std::setw(2) << (int)cpu.get_opcode()
                      << " step=" << (int)cpu.get_cycle_step();
            BUS_SET_DATA(bus_state, read_data);
        }
        
        BUS_SET_ADDR(bus_state, addr);
        
        // CRITICAL: Set RDY line high (ready) to prevent RDY wait
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        // Store state before cycle_tick
        uint32_t state_before = cpu.get_state_flags();
        
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check state after cycle_tick
        uint32_t state_after = cpu.get_state_flags();
        
        std::cout << " state_before=0x" << std::hex << std::setw(8) << std::setfill('0') << state_before
                  << " state_after=0x" << std::setw(8) << state_after << std::dec << std::endl;
        
        // Check if we've moved to next instruction
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            std::cout << "Instruction completed!" << std::endl;
            break;
        }
    }
    
    uint16_t final_pc = cpu.get_pc();
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  P=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p();
    std::cout << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << (int)final_pc;
    std::cout << " Flags=0x" << std::hex << std::setw(8) << std::setfill('0') << (int)cpu.get_state_flags() << std::dec << std::endl;
    
    return 0;
}