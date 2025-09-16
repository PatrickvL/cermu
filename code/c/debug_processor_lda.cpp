#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

// Simple 6502 configuration
using Config = config_6502;
using CpuType = fam65xx_with_cycle_count<Config>;

int main() {
    std::cout << "=== ProcessorTest-style LDA Debug ===" << std::endl;
    
    CpuType cpu;
    cpu.init_for_test();
    
    // Set up initial state like a ProcessorTest (simulating test "a9 b2 cb")
    // This test expects: P = 0xa0, but we get P = 0xa2
    cpu.set_pc(0x4a96);  // Initial PC from failing test
    cpu.set_a(0x00);     // Initial A register
    cpu.set_p(0xa2);     // Initial processor status from test
    
    std::cout << "--- Initial State ---" << std::endl;
    std::cout << "PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    
    // Simulate memory containing LDA #$b2 instruction
    bus_state_t bus_state = 0;
    
    // Cycle 1: Opcode fetch (should read $A9)
    BUS_SET_DATA(bus_state, 0xA9);
    std::cout << "\n--- Cycle 1: Opcode Fetch ---" << std::endl;
    std::cout << "Bus data: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)BUS_GET_DATA(bus_state) << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After opcode fetch:" << std::endl;
    std::cout << "PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    
    // Cycle 2: Operand fetch (should read $b2)
    BUS_SET_DATA(bus_state, 0xB2);
    std::cout << "\n--- Cycle 2: Operand Fetch ---" << std::endl;
    std::cout << "Bus data: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)BUS_GET_DATA(bus_state) << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After LDA execution:" << std::endl;
    std::cout << "PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    
    // Expected vs actual
    std::cout << "\n--- Expected vs Actual ---" << std::endl;
    std::cout << "Expected A: 0xb2, Got A: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << std::endl;
    std::cout << "Expected P: 0xa0, Got P: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    
    // Flag analysis
    uint8_t final_p = cpu.get_p();
    std::cout << "\n--- Flag Analysis ---" << std::endl;
    std::cout << "N flag (0x80): " << ((final_p & 0x80) ? "SET" : "CLEAR") << std::endl;
    std::cout << "V flag (0x40): " << ((final_p & 0x40) ? "SET" : "CLEAR") << std::endl;
    std::cout << "U flag (0x20): " << ((final_p & 0x20) ? "SET" : "CLEAR") << std::endl;
    std::cout << "B flag (0x10): " << ((final_p & 0x10) ? "SET" : "CLEAR") << std::endl;
    std::cout << "D flag (0x08): " << ((final_p & 0x08) ? "SET" : "CLEAR") << std::endl;
    std::cout << "I flag (0x04): " << ((final_p & 0x04) ? "SET" : "CLEAR") << std::endl;
    std::cout << "Z flag (0x02): " << ((final_p & 0x02) ? "SET" : "CLEAR") << std::endl;
    std::cout << "C flag (0x01): " << ((final_p & 0x01) ? "SET" : "CLEAR") << std::endl;
    
    return 0;
}