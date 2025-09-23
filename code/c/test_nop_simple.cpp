#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

bool test_nop_operation(uint8_t opcode, const char* name, const std::vector<uint8_t>& instruction_bytes) {
    TestCPU cpu;
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    cpu.set_p(0x24); // Clear all flags except unused
    
    std::array<uint8_t, 65536> memory = {};
    
    // Set up instruction
    for (size_t i = 0; i < instruction_bytes.size(); i++) {
        memory[0x1000 + i] = instruction_bytes[i];
    }
    
    // Store initial register state
    uint16_t initial_pc = cpu.get_pc();
    uint8_t initial_a = cpu.get_a();
    uint8_t initial_x = cpu.get_x();
    uint8_t initial_y = cpu.get_y();
    uint8_t initial_p = cpu.get_p();
    uint8_t initial_s = cpu.get_s();
    
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    int cycles = 0;
    
    do {
        uint16_t test_addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        if (is_read) {
            uint8_t data = memory[test_addr];
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            uint8_t data = cpu.get_write_data();
            memory[test_addr] = data;
            bus_state = BUS_SET_DATA(bus_state, data);
        }
        
        bus_state = cpu.cycle_tick(bus_state);
        cycles++;
        
    } while (cpu.get_cycle_step() != 0 && cycles < 10);
    
    // Check that PC advanced by the correct amount
    uint16_t expected_pc = initial_pc + instruction_bytes.size();
    bool pc_correct = (cpu.get_pc() == expected_pc);
    
    // Check that registers are unchanged
    bool registers_unchanged = (
        cpu.get_a() == initial_a &&
        cpu.get_x() == initial_x &&
        cpu.get_y() == initial_y &&
        cpu.get_p() == initial_p &&
        cpu.get_s() == initial_s
    );
    
    std::cout << name << " (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)opcode << "): ";
    if (pc_correct && registers_unchanged) {
        std::cout << "✅ PASS";
    } else {
        std::cout << "❌ FAIL";
        if (!pc_correct) {
            std::cout << " (PC: expected 0x" << std::hex << expected_pc 
                      << ", got 0x" << cpu.get_pc() << ")";
        }
        if (!registers_unchanged) {
            std::cout << " (registers changed)";
        }
    }
    std::cout << std::endl;
    
    return pc_correct && registers_unchanged;
}

int main() {
    std::cout << "=== Testing NOP Variants (Priority 7) ===" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // Test documented NOP (0xEA)
    total++; if (test_nop_operation(0xEA, "NOP", {0xEA})) passed++;
    
    // Test undocumented single-byte NOPs
    total++; if (test_nop_operation(0x1A, "NOP", {0x1A})) passed++;
    total++; if (test_nop_operation(0x3A, "NOP", {0x3A})) passed++;
    total++; if (test_nop_operation(0x5A, "NOP", {0x5A})) passed++;
    total++; if (test_nop_operation(0x7A, "NOP", {0x7A})) passed++;
    total++; if (test_nop_operation(0xDA, "NOP", {0xDA})) passed++;
    total++; if (test_nop_operation(0xFA, "NOP", {0xFA})) passed++;
    
    // Test undocumented 2-byte NOPs (immediate mode)
    total++; if (test_nop_operation(0x80, "NOP #$42", {0x80, 0x42})) passed++;
    total++; if (test_nop_operation(0x82, "NOP #$42", {0x82, 0x42})) passed++;
    total++; if (test_nop_operation(0x89, "NOP #$42", {0x89, 0x42})) passed++;
    total++; if (test_nop_operation(0xC2, "NOP #$42", {0xC2, 0x42})) passed++;
    total++; if (test_nop_operation(0xE2, "NOP #$42", {0xE2, 0x42})) passed++;
    
    // Test undocumented 2-byte NOPs (zero page)
    total++; if (test_nop_operation(0x04, "NOP $80", {0x04, 0x80})) passed++;
    total++; if (test_nop_operation(0x44, "NOP $80", {0x44, 0x80})) passed++;
    total++; if (test_nop_operation(0x64, "NOP $80", {0x64, 0x80})) passed++;
    
    // Test undocumented 3-byte NOPs (absolute)
    total++; if (test_nop_operation(0x0C, "NOP $2000", {0x0C, 0x00, 0x20})) passed++;
    
    // Test undocumented 2-byte NOPs (zero page,X)
    total++; if (test_nop_operation(0x14, "NOP $80,X", {0x14, 0x80})) passed++;
    total++; if (test_nop_operation(0x34, "NOP $80,X", {0x34, 0x80})) passed++;
    total++; if (test_nop_operation(0x54, "NOP $80,X", {0x54, 0x80})) passed++;
    total++; if (test_nop_operation(0x74, "NOP $80,X", {0x74, 0x80})) passed++;
    total++; if (test_nop_operation(0xD4, "NOP $80,X", {0xD4, 0x80})) passed++;
    total++; if (test_nop_operation(0xF4, "NOP $80,X", {0xF4, 0x80})) passed++;
    
    // Test undocumented 3-byte NOPs (absolute,X)
    total++; if (test_nop_operation(0x1C, "NOP $2000,X", {0x1C, 0x00, 0x20})) passed++;
    total++; if (test_nop_operation(0x3C, "NOP $2000,X", {0x3C, 0x00, 0x20})) passed++;
    total++; if (test_nop_operation(0x5C, "NOP $2000,X", {0x5C, 0x00, 0x20})) passed++;
    total++; if (test_nop_operation(0x7C, "NOP $2000,X", {0x7C, 0x00, 0x20})) passed++;
    total++; if (test_nop_operation(0xDC, "NOP $2000,X", {0xDC, 0x00, 0x20})) passed++;
    total++; if (test_nop_operation(0xFC, "NOP $2000,X", {0xFC, 0x00, 0x20})) passed++;
    
    std::cout << "\n=== Priority 7 Results ===" << std::endl;
    std::cout << "Passed: " << std::dec << passed << "/" << total << std::endl;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(1) 
              << (100.0 * passed / total) << "%" << std::endl;
    
    if (passed == total) {
        std::cout << "\n🎉 PRIORITY 7 COMPLETED! All NOP variants working!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some NOP variants need fixes" << std::endl;
        return 1;
    }
}