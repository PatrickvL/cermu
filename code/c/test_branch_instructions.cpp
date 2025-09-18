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

void test_branch_instruction(const std::string& name, uint8_t opcode, uint8_t offset, 
                            uint8_t initial_flags, bool expect_branch) {
    std::cout << "\n=== Testing " << name << " (0x" << std::hex << std::setw(2) 
              << std::setfill('0') << (int)opcode << ") ===" << std::endl;
    
    // Create CPU with 6502 configuration
    using CpuConfig = cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, '\x10', false, false, false>;
    fam65xx<CpuConfig> cpu;
    TestMemory mem;
    
    // Initialize CPU for testing
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_pc(0x1000);   // Set PC to test program
    cpu.set_p(initial_flags);  // Set processor status
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  P=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p();
    std::cout << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.get_pc() << std::dec << std::endl;
    
    // Set up memory with the test instruction
    mem.write(0x1000, opcode);  // Branch opcode
    mem.write(0x1001, offset);  // Branch offset
    mem.write(0x1002, 0xEA);    // NOP (not taken path)
    mem.write(0x1003, 0xEA);    // NOP
    mem.write(0x1004, 0xEA);    // NOP (branch target for +2 offset)
    
    // Simulate instruction execution
    bus_state_t bus_state = 0;
    
    std::cout << "\nExecuting branch instruction:" << std::endl;
    
    // Execute several cycles to complete branch instruction
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        if (!is_write) {
            uint8_t read_data = mem.read(addr);
            std::cout << "Cycle " << cycle << ": READ  addr=0x" 
                      << std::hex << std::setw(4) << std::setfill('0') << addr
                      << " data=0x" << std::setw(2) << (int)read_data 
                      << " opcode=0x" << std::setw(2) << (int)cpu.get_opcode()
                      << " step=" << (int)cpu.get_cycle_step() << std::dec << std::endl;
            BUS_SET_DATA(bus_state, read_data);
        }
        
        BUS_SET_ADDR(bus_state, addr);
        
        // CRITICAL: Set RDY line high (ready) to prevent RDY wait
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if we've moved to next instruction
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            break;
        }
    }
    
    uint16_t final_pc = cpu.get_pc();
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  P=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p();
    std::cout << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << (int)final_pc << std::dec << std::endl;
    
    // Analyze result
    uint16_t expected_pc;
    if (expect_branch) {
        expected_pc = 0x1002 + (int8_t)offset;  // 0x1002 + offset (after reading offset byte)
    } else {
        expected_pc = 0x1002;  // Next instruction after branch opcode + offset byte
    }
    
    std::cout << "Expected PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << expected_pc << std::dec;
    std::cout << " (branch " << (expect_branch ? "taken" : "not taken") << ")" << std::endl;
    
    if (final_pc == expected_pc) {
        std::cout << "✓ SUCCESS: Branch behavior correct!" << std::endl;
    } else {
        std::cout << "✗ FAILURE: Expected PC 0x" << std::hex << std::setw(4) << std::setfill('0') 
                  << expected_pc << ", got 0x" << std::setw(4) << final_pc << std::dec << std::endl;
    }
}

int main() {
    std::cout << "=== BRANCH INSTRUCTIONS DEBUG TEST ===" << std::endl;
    
    // Test BPL (Branch if Plus) - 0x10
    // N flag clear (0) = branch taken, N flag set (0x80) = branch not taken
    test_branch_instruction("BPL", 0x10, 0x02, 0x00, true);   // N=0, should branch
    test_branch_instruction("BPL", 0x10, 0x02, 0x80, false);  // N=1, should not branch
    
    // Test BMI (Branch if Minus) - 0x30  
    // N flag set (0x80) = branch taken, N flag clear (0) = branch not taken
    test_branch_instruction("BMI", 0x30, 0x02, 0x80, true);   // N=1, should branch
    test_branch_instruction("BMI", 0x30, 0x02, 0x00, false);  // N=0, should not branch
    
    // Test BEQ (Branch if Equal) - 0xF0
    // Z flag set (0x02) = branch taken, Z flag clear (0) = branch not taken  
    test_branch_instruction("BEQ", 0xF0, 0x02, 0x02, true);   // Z=1, should branch
    test_branch_instruction("BEQ", 0xF0, 0x02, 0x00, false);  // Z=0, should not branch
    
    return 0;
}