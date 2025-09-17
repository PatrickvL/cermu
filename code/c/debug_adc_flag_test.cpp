#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cycle_table_gen.hpp"
#include <iostream>
#include <iomanip>

int main() {
    using namespace fam65xx_cpp;
    
    // Test one specific failing case: 69 d5 74
    // This should help us debug the exact flag issue
    
    fam65xx_with_cycle_count<config_6502> cpu;
    cpu.init_for_test();
    
    // Setup initial state from ProcessorTests
    cpu.set_pc(0x2eaa);  // From failing test
    cpu.set_a(0xd5);     // First operand
    cpu.set_p(0x74);     // Initial flags
    
    std::cout << "=== ADC Flag Debug Test ===\n";
    std::cout << "Testing: ADC #$d5 with A=$d5, P=$74\n";
    std::cout << "Initial state:\n";
    std::cout << "  A = 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_a() << "\n";
    std::cout << "  P = 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_p() << "\n";
    
    // Check flag breakdown
    uint8_t initial_p = cpu.get_p();
    std::cout << "  Flag breakdown: N=" << ((initial_p & 0x80) ? 1 : 0) 
              << " V=" << ((initial_p & 0x40) ? 1 : 0)
              << " B=" << ((initial_p & 0x10) ? 1 : 0)
              << " D=" << ((initial_p & 0x08) ? 1 : 0)
              << " I=" << ((initial_p & 0x04) ? 1 : 0)
              << " Z=" << ((initial_p & 0x02) ? 1 : 0)
              << " C=" << ((initial_p & 0x01) ? 1 : 0) << "\n";
    
    // Manual calculation for verification
    uint8_t a = 0xd5;
    uint8_t data = 0xd5;
    uint8_t carry_in = (initial_p & 0x01) ? 1 : 0;  // C=0 from P=0x74
    uint16_t temp = a + data + carry_in;
    uint8_t result = temp & 0xFF;
    
    std::cout << "\nManual calculation:\n";
    std::cout << "  0x" << std::hex << (int)a << " + 0x" << (int)data << " + " << (int)carry_in << " = 0x" << temp << "\n";
    std::cout << "  Result = 0x" << (int)result << "\n";
    std::cout << "  Should set C = " << (temp > 0xFF ? 1 : 0) << "\n";
    std::cout << "  Should set V = " << ((~(a ^ data) & (a ^ result) & 0x80) ? 1 : 0) << "\n";
    std::cout << "  Should set N = " << ((result & 0x80) ? 1 : 0) << "\n";
    std::cout << "  Should set Z = " << ((result == 0) ? 1 : 0) << "\n";
    
    // Expected final P value (ProcessorTests says 0xad)
    std::cout << "\nExpected final P = 0xad\n";
    std::cout << "  Expected: N=1 V=0 B=1 D=1 I=1 Z=0 C=1\n";
    
    return 0;
}