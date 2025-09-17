#include <iostream>
#include <iomanip>
#include <cassert>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace std;

class BranchTestHarness {
private:
    fam65xx_cpp::fam65xx<config_6502>* cpu;
    uint8_t memory[65536];

public:
    BranchTestHarness() {
        cpu = new fam65xx_cpp::fam65xx<config_6502>();
        // Initialize memory to zero
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
    }
    
    ~BranchTestHarness() {
        delete cpu;
    }

    void write_memory(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
    
    uint8_t read_memory(uint16_t addr) {
        return memory[addr];
    }

    void test_branch_instruction(const string& name, uint8_t opcode, uint8_t initial_flags,
                               bool should_branch, const string& description) {
        cout << "Test: " << name << " - " << description << endl;
        
        // Setup test
        cpu->init_for_test();
        cpu->set_p(initial_flags);
        cpu->set_pc(0x1000);
        
        // Set up memory: opcode at PC, branch offset at PC+1
        write_memory(0x1000, opcode);  // Branch opcode
        write_memory(0x1001, 0x05);   // Branch offset (+5)
        
        cout << "  Initial: PC=0x" << hex << cpu->get_pc()
             << " P=0x" << hex << (int)cpu->get_p() << endl;
        
        // Execute instruction cycle by cycle
        bus_state_t bus = 0;
        int cycles = 0;
        
        // Cycle 0: Opcode fetch
        BUS_SET_ADDR(bus, cpu->get_address());
        BUS_SET_DATA(bus, read_memory(cpu->get_address()));
        cout << "  Cycle " << cycles << ": addr=0x" << hex << cpu->get_address()
             << " data=0x" << hex << (int)read_memory(cpu->get_address()) << endl;
        bus = cpu->cycle_tick(bus);
        cycles++;
        
        // Cycle 1: Operand fetch
        if (cpu->get_cycle_step() != 0) {
            BUS_SET_ADDR(bus, cpu->get_address());
            BUS_SET_DATA(bus, read_memory(cpu->get_address()));
            cout << "  Cycle " << cycles << ": addr=0x" << hex << cpu->get_address()
                 << " data=0x" << hex << (int)read_memory(cpu->get_address()) << endl;
            bus = cpu->cycle_tick(bus);
            cycles++;
        }
        
        // Additional cycles if branch taken
        while (cpu->get_cycle_step() != 0 && cycles < 10) {
            BUS_SET_ADDR(bus, cpu->get_address());
            BUS_SET_DATA(bus, read_memory(cpu->get_address()));
            cout << "  Cycle " << cycles << ": addr=0x" << hex << cpu->get_address()
                 << " data=0x" << hex << (int)read_memory(cpu->get_address()) << endl;
            bus = cpu->cycle_tick(bus);
            cycles++;
        }
        
        uint16_t expected_pc = should_branch ? 0x1007 : 0x1002;  // +5 offset or +2 for next instruction
        
        cout << "  Final:   PC=0x" << hex << cpu->get_pc()
             << " Cycles=" << dec << cycles << endl;
        cout << "  Expected: PC=0x" << hex << expected_pc
             << " Should branch: " << (should_branch ? "Yes" : "No") << endl;
        
        bool correct = (cpu->get_pc() == expected_pc);
        cout << "  Result: " << (correct ? "✅ PASS" : "❌ FAIL") << endl << endl;
    }
};

int main() {
    cout << "=== Branch Instructions Test ===" << endl;
    cout << "Testing branch instruction implementation." << endl << endl;
    
    BranchTestHarness test;
    
    // Test BPL (Branch if Plus/Positive) - 0x10
    // Branches if N flag is clear (bit 7 = 0)
    test.test_branch_instruction("BPL", 0x10, 0x00, true, "N=0, should branch");
    test.test_branch_instruction("BPL", 0x10, 0x80, false, "N=1, should not branch"); 
    
    // Test BMI (Branch if Minus/Negative) - 0x30  
    // Branches if N flag is set (bit 7 = 1)
    test.test_branch_instruction("BMI", 0x30, 0x80, true, "N=1, should branch");
    test.test_branch_instruction("BMI", 0x30, 0x00, false, "N=0, should not branch");
    
    // Test BVC (Branch if Overflow Clear) - 0x50
    // Branches if V flag is clear (bit 6 = 0) 
    test.test_branch_instruction("BVC", 0x50, 0x00, true, "V=0, should branch");
    test.test_branch_instruction("BVC", 0x50, 0x40, false, "V=1, should not branch");
    
    // Test BVS (Branch if Overflow Set) - 0x70
    // Branches if V flag is set (bit 6 = 1)
    test.test_branch_instruction("BVS", 0x70, 0x40, true, "V=1, should branch");
    test.test_branch_instruction("BVS", 0x70, 0x00, false, "V=0, should not branch");
    
    // Test BCC (Branch if Carry Clear) - 0x90
    // Branches if C flag is clear (bit 0 = 0)
    test.test_branch_instruction("BCC", 0x90, 0x00, true, "C=0, should branch");
    test.test_branch_instruction("BCC", 0x90, 0x01, false, "C=1, should not branch");
    
    // Test BCS (Branch if Carry Set) - 0xB0
    // Branches if C flag is set (bit 0 = 1)
    test.test_branch_instruction("BCS", 0xB0, 0x01, true, "C=1, should branch");
    test.test_branch_instruction("BCS", 0xB0, 0x00, false, "C=0, should not branch");
    
    // Test BNE (Branch if Not Equal) - 0xD0
    // Branches if Z flag is clear (bit 1 = 0)
    test.test_branch_instruction("BNE", 0xD0, 0x00, true, "Z=0, should branch");
    test.test_branch_instruction("BNE", 0xD0, 0x02, false, "Z=1, should not branch");
    
    // Test BEQ (Branch if Equal) - 0xF0
    // Branches if Z flag is set (bit 1 = 1)
    test.test_branch_instruction("BEQ", 0xF0, 0x02, true, "Z=1, should branch");
    test.test_branch_instruction("BEQ", 0xF0, 0x00, false, "Z=0, should not branch");
    
    cout << "=== Branch Tests Complete ===" << endl;
    cout << "🔧 Next: Analyze results and fix branch instruction implementation" << endl;
    
    return 0;
}