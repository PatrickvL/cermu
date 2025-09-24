#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <cstring>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

struct TestCase {
    uint8_t opcode;
    std::vector<uint8_t> operands;
    const char* name;
};

// Test all remaining potentially failing opcodes
std::vector<TestCase> get_test_cases() {
    std::vector<TestCase> cases;
    
    // All 256 possible opcodes - we'll test everything and see what fails
    for (int op = 0x00; op <= 0xFF; op++) {
        uint8_t opcode = static_cast<uint8_t>(op);
        
        // Skip opcodes we know are working (from previous priorities)
        // Skip documented working opcodes based on our previous success
        if (
            // Skip Transfer operations (Priority 1)
            opcode == 0xAA || opcode == 0x8A || opcode == 0xA8 || opcode == 0x98 || 
            opcode == 0xBA || opcode == 0x9A ||
            
            // Skip Flag operations (Priority 2) 
            opcode == 0x18 || opcode == 0x38 || opcode == 0x58 || opcode == 0x78 ||
            opcode == 0xB8 || opcode == 0xD8 || opcode == 0xF8 ||
            
            // Skip some known working ADC/SBC operations (Priority 3)
            opcode == 0x69 || opcode == 0x65 || opcode == 0x75 || opcode == 0x6D || opcode == 0x7D || opcode == 0x79 ||
            opcode == 0x61 || opcode == 0x71 || opcode == 0xE9 || opcode == 0xE5 || opcode == 0xF5 || opcode == 0xED ||
            opcode == 0xFD || opcode == 0xF9 || opcode == 0xE1 || opcode == 0xF1 ||
            
            // Skip Accumulator operations (Priority 4)
            opcode == 0x0A || opcode == 0x4A || opcode == 0x2A || opcode == 0x6A ||
            
            // Skip some known working shift/rotate memory operations (Priority 5)
            opcode == 0x06 || opcode == 0x16 || opcode == 0x0E || opcode == 0x1E ||
            opcode == 0x46 || opcode == 0x56 || opcode == 0x4E || opcode == 0x5E ||
            opcode == 0x26 || opcode == 0x36 || opcode == 0x2E || opcode == 0x3E ||
            opcode == 0x66 || opcode == 0x76 || opcode == 0x6E || opcode == 0x7E ||
            
            // Skip memory inc/dec operations (Priority 6)
            opcode == 0xE6 || opcode == 0xF6 || opcode == 0xEE || opcode == 0xFE ||
            opcode == 0xC6 || opcode == 0xD6 || opcode == 0xCE || opcode == 0xDE ||
            
            // Skip some known working control flow
            opcode == 0x4C || opcode == 0x6C || opcode == 0x20 || opcode == 0x60 || opcode == 0x40 ||
            
            // Skip some known working branches  
            opcode == 0x10 || opcode == 0x30 || opcode == 0x50 || opcode == 0x70 ||
            opcode == 0x90 || opcode == 0xB0 || opcode == 0xD0 || opcode == 0xF0 ||
            
            // Skip some known working loads/stores
            opcode == 0xA9 || opcode == 0xA5 || opcode == 0xB5 || opcode == 0xAD || opcode == 0xBD || opcode == 0xB9 ||
            opcode == 0xA1 || opcode == 0xB1 || // LDA
            opcode == 0xA2 || opcode == 0xA6 || opcode == 0xB6 || opcode == 0xAE || opcode == 0xBE || // LDX  
            opcode == 0xA0 || opcode == 0xA4 || opcode == 0xB4 || opcode == 0xAC || opcode == 0xBC || // LDY
            
            // Skip stack operations
            opcode == 0x48 || opcode == 0x68 || opcode == 0x08 || opcode == 0x28
        ) {
            continue; // Skip known working opcodes
        }
        
        // Test remaining opcodes
        std::vector<uint8_t> operands;
        
        // Add operands based on likely addressing modes
        if (opcode == 0x00) { // BRK
            cases.push_back({opcode, {}, "BRK"});
        } else {
            // For most opcodes, provide 2 bytes of operand data
            operands = {0x80, 0x20}; // Common test values
            cases.push_back({opcode, operands, "Unknown"});
        }
    }
    
    return cases;
}

bool test_instruction(uint8_t opcode, const std::vector<uint8_t>& operands, const char* name) {
    TestCPU cpu;
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    
    std::array<uint8_t, 65536> memory = {};
    
    // Set up instruction
    memory[0x1000] = opcode;
    for (size_t i = 0; i < operands.size(); i++) {
        memory[0x1001 + i] = operands[i];
    }
    
    // Store initial state
    uint16_t initial_pc = cpu.get_pc();
    uint8_t initial_a = cpu.get_a();
    uint8_t initial_x = cpu.get_x(); 
    uint8_t initial_y = cpu.get_y();
    uint8_t initial_p = cpu.get_p();
    uint8_t initial_s = cpu.get_s();
    
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    
    int cycles = 0;
    bool instruction_completed = false;
    
    try {
        while (cycles < 20 && !instruction_completed) { // Limit cycles to prevent hangs
            uint16_t addr = cpu.get_address();
            bool is_read = cpu.get_rw();
            
            if (is_read) {
                uint8_t data = memory[addr];
                bus_state = BUS_SET_DATA(bus_state, data);
            } else {
                uint8_t data = cpu.get_write_data();
                memory[addr] = data;
                bus_state = BUS_SET_DATA(bus_state, data);
            }
            
            bus_state = cpu.cycle_tick(bus_state);
            cycles++;
            
            // Check if instruction completed (cycle_step returned to 0)
            if (cpu.get_cycle_step() == 0 && cycles > 1) {
                instruction_completed = true;
            }
        }
        
        if (!instruction_completed) {
            std::printf("0x%02X (%s): ❌ TIMEOUT (>20 cycles)\n", opcode, name);
            return false;
        }
        
        std::printf("0x%02X (%s): ✅ EXECUTED (%d cycles)\n", opcode, name, cycles);
        return true;
        
    } catch (...) {
        std::printf("0x%02X (%s): ❌ EXCEPTION\n", opcode, name);
        return false;
    }
}

int main() {
    std::cout << "=== Testing Remaining Failing Instructions ===" << std::endl;
    
    auto test_cases = get_test_cases();
    int passed = 0;
    int total = test_cases.size();
    
    for (const auto& test_case : test_cases) {
        if (test_instruction(test_case.opcode, test_case.operands, test_case.name)) {
            passed++;
        }
    }
    
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Passed: " << passed << "/" << total << std::endl;
    std::cout << "Failed: " << (total - passed) << "/" << total << std::endl;
    std::cout << "Success Rate: " << (100.0 * passed / total) << "%" << std::endl;
    
    return (passed == total) ? 0 : 1;
}