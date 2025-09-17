// Comprehensive test for ALL transfer and flag instructions after systematic fix
#include <iostream>
#include <iomanip>
#include <vector>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

struct InstructionTest {
    uint8_t opcode;
    const char* mnemonic;
    uint8_t init_a, init_x, init_y, init_s, init_p;
    uint8_t expect_a, expect_x, expect_y, expect_s, expect_p_mask, expect_p_value;
    const char* description;
};

int main() {
    std::cout << "=== Comprehensive Transfer and Flag Instructions Test ===\n";
    std::cout << "Testing ALL transfer and flag instructions that were previously failing.\n\n";
    
    // Use the same CPU configuration as ProcessorTests
    fam65xx_with_cycle_count<config_6502> cpu;
    
    // Memory array for testing
    uint8_t memory[0x10000];
    std::fill(memory, memory + sizeof(memory), 0xEA); // Fill with NOP
    
    // Test cases for ALL transfer and flag instructions
    std::vector<InstructionTest> tests = {
        // === TRANSFER INSTRUCTIONS ===
        {0x8A, "TXA", 0x00, 0x42, 0x00, 0xFF, 0x00, 0x42, 0x42, 0x00, 0xFF, 0x82, 0x00, "TXA: X=0x42 -> A, expect N=0 Z=0"},
        {0x8A, "TXA", 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x82, 0x02, "TXA: X=0x00 -> A, expect N=0 Z=1"},
        {0x8A, "TXA", 0x00, 0x80, 0x00, 0xFF, 0x00, 0x80, 0x80, 0x00, 0xFF, 0x82, 0x80, "TXA: X=0x80 -> A, expect N=1 Z=0"},
        
        {0xAA, "TAX", 0x42, 0x00, 0x00, 0xFF, 0x00, 0x42, 0x42, 0x00, 0xFF, 0x82, 0x00, "TAX: A=0x42 -> X, expect N=0 Z=0"},
        {0xAA, "TAX", 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x82, 0x02, "TAX: A=0x00 -> X, expect N=0 Z=1"},
        {0xAA, "TAX", 0x80, 0x00, 0x00, 0xFF, 0x00, 0x80, 0x80, 0x00, 0xFF, 0x82, 0x80, "TAX: A=0x80 -> X, expect N=1 Z=0"},
        
        {0x98, "TYA", 0x00, 0x00, 0x42, 0xFF, 0x00, 0x42, 0x00, 0x42, 0xFF, 0x82, 0x00, "TYA: Y=0x42 -> A, expect N=0 Z=0"},
        {0x98, "TYA", 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x82, 0x02, "TYA: Y=0x00 -> A, expect N=0 Z=1"},
        {0x98, "TYA", 0x00, 0x00, 0x80, 0xFF, 0x00, 0x80, 0x00, 0x80, 0xFF, 0x82, 0x80, "TYA: Y=0x80 -> A, expect N=1 Z=0"},
        
        {0xA8, "TAY", 0x42, 0x00, 0x00, 0xFF, 0x00, 0x42, 0x00, 0x42, 0xFF, 0x82, 0x00, "TAY: A=0x42 -> Y, expect N=0 Z=0"},
        {0xA8, "TAY", 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x82, 0x02, "TAY: A=0x00 -> Y, expect N=0 Z=1"},
        {0xA8, "TAY", 0x80, 0x00, 0x00, 0xFF, 0x00, 0x80, 0x00, 0x80, 0xFF, 0x82, 0x80, "TAY: A=0x80 -> Y, expect N=1 Z=0"},
        
        {0xBA, "TSX", 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x42, 0x00, 0x42, 0x82, 0x00, "TSX: S=0x42 -> X, expect N=0 Z=0"},
        {0xBA, "TSX", 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x02, "TSX: S=0x00 -> X, expect N=0 Z=1"},
        {0xBA, "TSX", 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x80, 0x82, 0x80, "TSX: S=0x80 -> X, expect N=1 Z=0"},
        
        {0x9A, "TXS", 0x00, 0x42, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x42, 0x82, 0x00, "TXS: X=0x42 -> S, NO flags set"},
        {0x9A, "TXS", 0x00, 0x00, 0x00, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x00, 0x82, 0x80, "TXS: X=0x00 -> S, NO flags set (preserves N)"},
        
        // === FLAG INSTRUCTIONS ===
        {0x18, "CLC", 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x01, 0x00, "CLC: Clear carry flag"},
        {0x38, "SEC", 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0x01, "SEC: Set carry flag"},
        
        {0x58, "CLI", 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x04, 0x00, "CLI: Clear interrupt disable"},
        {0x78, "SEI", 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x04, 0x04, "SEI: Set interrupt disable"},
        
        {0xB8, "CLV", 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x40, 0x00, "CLV: Clear overflow flag"},
        
        {0xD8, "CLD", 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x08, 0x00, "CLD: Clear decimal mode"},
        {0xF8, "SED", 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x08, 0x08, "SED: Set decimal mode"},
    };
    
    int passed = 0;
    int total = tests.size();
    
    for (size_t i = 0; i < tests.size(); i++) {
        const auto& test = tests[i];
        
        std::cout << "Test " << (i + 1) << ": " << test.description << "\n";
        
        // Set up instruction at address 0x0000
        memory[0x0000] = test.opcode;
        
        // Initialize CPU state
        cpu.init_for_test();
        cpu.set_a(test.init_a);
        cpu.set_x(test.init_x);
        cpu.set_y(test.init_y);
        cpu.set_s(test.init_s);
        cpu.set_p(test.init_p);
        cpu.set_pc(0x0000);
        
        std::cout << "  Initial: A=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_a() 
                  << " X=0x" << std::setw(2) << (int)cpu.get_x() 
                  << " Y=0x" << std::setw(2) << (int)cpu.get_y() 
                  << " S=0x" << std::setw(2) << (int)cpu.get_s() 
                  << " P=0x" << std::setw(2) << (int)cpu.get_p() << std::dec << "\n";
        
        // Execute instruction cycle by cycle
        bus_state_t bus_state = 0;
        
        // Cycle 1: Opcode fetch
        bus_state = BUS_SET_DATA(bus_state, memory[cpu.get_address()]);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 2: Execute (all these are single-cycle implied instructions)
        bus_state = BUS_SET_DATA(bus_state, memory[cpu.get_address()]);
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "  Final:   A=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_a() 
                  << " X=0x" << std::setw(2) << (int)cpu.get_x() 
                  << " Y=0x" << std::setw(2) << (int)cpu.get_y() 
                  << " S=0x" << std::setw(2) << (int)cpu.get_s() 
                  << " P=0x" << std::setw(2) << (int)cpu.get_p() << std::dec << "\n";
        
        // Check results
        bool test_passed = (cpu.get_a() == test.expect_a) && 
                          (cpu.get_x() == test.expect_x) && 
                          (cpu.get_y() == test.expect_y) && 
                          (cpu.get_s() == test.expect_s) && 
                          ((cpu.get_p() & test.expect_p_mask) == test.expect_p_value);
        
        std::cout << "  Expected: A=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)test.expect_a 
                  << " X=0x" << std::setw(2) << (int)test.expect_x 
                  << " Y=0x" << std::setw(2) << (int)test.expect_y 
                  << " S=0x" << std::setw(2) << (int)test.expect_s 
                  << " P & 0x" << std::setw(2) << (int)test.expect_p_mask << " = 0x" << std::setw(2) << (int)test.expect_p_value << std::dec << "\n";
        
        std::cout << "  Result: " << (test_passed ? "✅ PASS" : "❌ FAIL") << "\n\n";
        
        if (test_passed) passed++;
    }
    
    std::cout << "=== Test Summary ===\n";
    std::cout << "Passed: " << passed << "/" << total << " tests (" 
              << (passed * 100 / total) << "%)\n";
    
    if (passed == total) {
        std::cout << "\n🎉 ALL TESTS PASSED!\n";
        std::cout << "✅ Transfer instructions (TAX/TXA/TAY/TYA/TSX/TXS) working correctly!\n";
        std::cout << "✅ Flag instructions (CLC/SEC/CLI/SEI/CLV/CLD/SED) working correctly!\n";
        std::cout << "🔧 Fixed systematic 0% pass rate issue for non-immediate instructions!\n";
        return 0;
    } else {
        std::cout << "\n❌ Some tests failed. Need further debugging.\n";
        return 1;
    }
}