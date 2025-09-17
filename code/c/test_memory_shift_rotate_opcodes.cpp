// Test for memory-mode shift/rotate opcodes (ASL/LSR/ROL/ROR $nn)
#include <iostream>
#include <iomanip>
#include <vector>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

struct MemoryShiftTest {
    uint8_t opcode;
    const char* mnemonic;
    const char* mode;
    uint16_t target_addr;
    uint8_t setup_data;
    uint8_t init_carry;
    uint8_t expect_result;
    uint8_t expect_carry;
    uint8_t expect_n_flag;
    uint8_t expect_z_flag;
    int cycles;
    const char* description;
};

int main() {
    std::cout << "=== Memory-Mode Shift/Rotate Instructions Test ===\n";
    std::cout << "Testing memory-mode shift/rotate opcodes.\n\n";
    
    // Use the same CPU configuration as ProcessorTests
    fam65xx_with_cycle_count<config_6502> cpu;
    
    // Memory array for testing
    uint8_t memory[0x10000];
    std::fill(memory, memory + sizeof(memory), 0xEA); // Fill with NOP
    
    // Test cases for memory-mode shift/rotate instructions
    std::vector<MemoryShiftTest> tests = {
        // === ASL (Arithmetic Shift Left) Memory Modes ===
        {0x06, "ASL", "$nn", 0x0010, 0x40, 0, 0x80, 0, 1, 0, 5, "ASL $10: 0x40 << 1 = 0x80, C=0, N=1"},
        {0x06, "ASL", "$nn", 0x0010, 0x80, 0, 0x00, 1, 0, 1, 5, "ASL $10: 0x80 << 1 = 0x00, C=1, Z=1"},
        {0x06, "ASL", "$nn", 0x0010, 0x01, 0, 0x02, 0, 0, 0, 5, "ASL $10: 0x01 << 1 = 0x02, C=0, N=0"},
        
        // === LSR (Logical Shift Right) Memory Modes ===
        {0x46, "LSR", "$nn", 0x0010, 0x81, 0, 0x40, 1, 0, 0, 5, "LSR $10: 0x81 >> 1 = 0x40, C=1, N=0"},
        {0x46, "LSR", "$nn", 0x0010, 0x01, 0, 0x00, 1, 0, 1, 5, "LSR $10: 0x01 >> 1 = 0x00, C=1, Z=1"},
        {0x46, "LSR", "$nn", 0x0010, 0x02, 0, 0x01, 0, 0, 0, 5, "LSR $10: 0x02 >> 1 = 0x01, C=0, N=0"},
        
        // === ROL (Rotate Left) Memory Modes ===
        {0x26, "ROL", "$nn", 0x0010, 0x80, 0, 0x00, 1, 0, 1, 5, "ROL $10: 0x80 << 1 + C=0 = 0x00, C=1, Z=1"},
        {0x26, "ROL", "$nn", 0x0010, 0x40, 1, 0x81, 0, 1, 0, 5, "ROL $10: 0x40 << 1 + C=1 = 0x81, C=0, N=1"},
        {0x26, "ROL", "$nn", 0x0010, 0x01, 0, 0x02, 0, 0, 0, 5, "ROL $10: 0x01 << 1 + C=0 = 0x02, C=0, N=0"},
        
        // === ROR (Rotate Right) Memory Modes ===
        {0x66, "ROR", "$nn", 0x0010, 0x01, 0, 0x00, 1, 0, 1, 5, "ROR $10: 0x01 >> 1 + C=0 = 0x00, C=1, Z=1"},
        {0x66, "ROR", "$nn", 0x0010, 0x02, 1, 0x81, 0, 1, 0, 5, "ROR $10: 0x02 >> 1 + C=1 = 0x81, C=0, N=1"},
        {0x66, "ROR", "$nn", 0x0010, 0x04, 0, 0x02, 0, 0, 0, 5, "ROR $10: 0x04 >> 1 + C=0 = 0x02, C=0, N=0"},
    };
    
    int passed = 0;
    int total = tests.size();
    
    for (size_t i = 0; i < tests.size(); i++) {
        const auto& test = tests[i];
        
        std::cout << "Test " << (i + 1) << ": " << test.description << "\n";
        
        // Set up instruction at address 0x0000
        memory[0x0000] = test.opcode;
        memory[0x0001] = test.target_addr & 0xFF;  // Zero page address
        
        // Set up data at target address
        memory[test.target_addr] = test.setup_data;
        
        // Initialize CPU state
        cpu.init_for_test();
        cpu.set_p(test.init_carry ? 0x01 : 0x00);  // Set carry flag if needed
        cpu.set_pc(0x0000);
        
        std::cout << "  Initial: Memory[$" << std::hex << std::setfill('0') << std::setw(4) << test.target_addr 
                  << "]=0x" << std::setw(2) << (int)test.setup_data
                  << " C=" << (test.init_carry ? "1" : "0") << std::dec << "\n";
        
        // Execute instruction cycle by cycle
        bus_state_t bus_state = 0;
        int cycle_count = 0;
        
        do {
            cycle_count++;
            uint16_t addr = cpu.get_address();
            uint8_t data = memory[addr];
            
            // Handle writes
            if (!cpu.get_rw()) {
                memory[addr] = cpu.get_write_data();
                std::cout << "    Cycle " << cycle_count << ": WRITE 0x" 
                          << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_write_data()
                          << " to $" << std::setw(4) << addr << std::dec << "\n";
            } else {
                std::cout << "    Cycle " << cycle_count << ": READ 0x" 
                          << std::hex << std::setfill('0') << std::setw(2) << (int)data
                          << " from $" << std::setw(4) << addr << std::dec << "\n";
            }
            
            bus_state = BUS_SET_DATA(bus_state, data);
            bus_state = cpu.cycle_tick(bus_state);
            
        } while (cycle_count < test.cycles && cpu.get_cycle_step() != 0);
        
        uint8_t final_memory = memory[test.target_addr];
        
        std::cout << "  Final:   Memory[$" << std::hex << std::setfill('0') << std::setw(4) << test.target_addr 
                  << "]=0x" << std::setw(2) << (int)final_memory
                  << " C=" << ((cpu.get_p() & 0x01) ? "1" : "0")
                  << " N=" << ((cpu.get_p() & 0x80) ? "1" : "0")
                  << " Z=" << ((cpu.get_p() & 0x02) ? "1" : "0") 
                  << " Cycles=" << cycle_count << std::dec << "\n";
        
        // Check results
        bool test_passed = (final_memory == test.expect_result) && 
                          ((cpu.get_p() & 0x01) == test.expect_carry) &&
                          (((cpu.get_p() & 0x80) != 0) == (test.expect_n_flag != 0)) &&
                          (((cpu.get_p() & 0x02) != 0) == (test.expect_z_flag != 0));
        
        std::cout << "  Expected: Memory=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)test.expect_result 
                  << " C=" << (test.expect_carry ? "1" : "0")
                  << " N=" << (test.expect_n_flag ? "1" : "0")
                  << " Z=" << (test.expect_z_flag ? "1" : "0") << std::dec << "\n";
        
        std::cout << "  Result: " << (test_passed ? "✅ PASS" : "❌ FAIL") << "\n\n";
        
        if (test_passed) passed++;
    }
    
    std::cout << "=== Memory-Mode Results ===\n";
    std::cout << "Passed: " << passed << "/" << total << " tests (" 
              << (passed * 100 / total) << "%)\n\n";
    
    if (passed == total) {
        std::cout << "✅ All memory-mode shift/rotate operations working correctly!\n";
        return 0;
    } else {
        std::cout << "❌ Some memory-mode tests failed. Need to debug and fix.\n";
        return 1;
    }
}