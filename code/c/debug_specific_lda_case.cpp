#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

int main() {
    // Test the exact failing case: "a9 b2 cb"
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set initial state exactly as in the failing test
    cpu.set_pc(19094);
    cpu.set_s(237);
    cpu.set_a(178);
    cpu.set_x(129);
    cpu.set_y(142);
    cpu.set_p(162);  // 0xa2 - initial P register
    
    std::cout << "=== Debug Specific LDA Case: a9 b2 cb ===" << std::endl;
    std::cout << "--- Initial State (Exact ProcessorTest Values) ---" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "Expected: A already contains 178, loading 178 again" << std::endl;
    std::cout << "Expected P change: 0xa2 -> 0xa0 (Z flag should clear)" << std::endl;
    
    // Cycle 1: Opcode fetch (0xa9)
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0xa9);
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "\n--- After Cycle 1: Opcode Fetch ---" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Cycle 2: Operand fetch (0xb2 = 178)
    bus_state = BUS_SET_DATA(bus_state, 0xb2);
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "\n--- After Cycle 2: LDA #$B2 Execution ---" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    std::cout << "\n--- Expected vs Actual ---" << std::endl;
    std::cout << "Expected A: 0xb2, Got A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "Expected P: 0xa0, Got P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    std::cout << "\n--- Flag Analysis ---" << std::endl;
    uint8_t p = cpu.get_p();
    std::cout << "N flag (0x80): " << ((p & 0x80) ? "SET" : "CLEAR") << std::endl;
    std::cout << "V flag (0x40): " << ((p & 0x40) ? "SET" : "CLEAR") << std::endl;
    std::cout << "U flag (0x20): " << ((p & 0x20) ? "SET" : "CLEAR") << std::endl;
    std::cout << "B flag (0x10): " << ((p & 0x10) ? "SET" : "CLEAR") << std::endl;
    std::cout << "D flag (0x08): " << ((p & 0x08) ? "SET" : "CLEAR") << std::endl;
    std::cout << "I flag (0x04): " << ((p & 0x04) ? "SET" : "CLEAR") << std::endl;
    std::cout << "Z flag (0x02): " << ((p & 0x02) ? "SET" : "CLEAR") << " <- SHOULD BE CLEAR!" << std::endl;
    std::cout << "C flag (0x01): " << ((p & 0x01) ? "SET" : "CLEAR") << std::endl;
    
    std::cout << "\n--- Debugging Flag Calculation ---" << std::endl;
    uint8_t data = 0xb2;
    std::cout << "Data loaded: 0x" << std::hex << (int)data << " = " << std::dec << (int)data << std::endl;
    std::cout << "data == 0: " << (data == 0 ? "true" : "false") << std::endl;
    std::cout << "data & 0x80: 0x" << std::hex << (data & 0x80) << std::endl;
    std::cout << "Expected Z flag: " << ((data == 0) ? "SET" : "CLEAR") << std::endl;
    std::cout << "Expected N flag: " << ((data & 0x80) ? "SET" : "CLEAR") << std::endl;
    
    return 0;
}