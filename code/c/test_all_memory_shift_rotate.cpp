#include <iostream>
#include <iomanip>
#include <vector>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

struct TestCase {
    uint8_t opcode;
    const char* name;
    uint8_t operand;  // For zero page address or indexed offset
    uint8_t initial_value;
    uint8_t expected_result;
    bool expected_carry;
    bool expected_negative;
    int expected_cycles;
};

bool test_instruction(const TestCase& test) {
    TestCPU cpu;
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    
    // Set up initial flags for rotate operations
    if (test.name[0] == 'R') { // ROL or ROR
        cpu.set_p(0x24); // Clear carry for consistent rotate testing
    }
    
    std::array<uint8_t, 65536> memory = {};
    memory[0x1000] = test.opcode;
    memory[0x1001] = test.operand;
    
    // Set up memory and X register for different addressing modes
    if (test.opcode == 0x16 || test.opcode == 0x56 || test.opcode == 0x36 || test.opcode == 0x76) {
        // Zero page,X addressing
        cpu.set_x(0x05);  // X offset
        uint8_t target_addr = (test.operand + 0x05) & 0xFF;
        memory[target_addr] = test.initial_value;
    } else if (test.opcode == 0x1E || test.opcode == 0x5E || test.opcode == 0x3E || test.opcode == 0x7E) {
        // Absolute,X addressing
        cpu.set_x(0x05);  // X offset
        uint16_t base_addr = 0x2000 + test.operand;
        uint16_t target_addr = base_addr + 0x05;
        memory[0x1001] = base_addr & 0xFF;         // Low byte
        memory[0x1002] = (base_addr >> 8) & 0xFF;  // High byte
        memory[target_addr] = test.initial_value;
    } else {
        // Zero page or absolute addressing
        if (test.opcode == 0x06 || test.opcode == 0x46 || test.opcode == 0x26 || test.opcode == 0x66) {
            // Zero page
            memory[test.operand] = test.initial_value;
        } else {
            // Absolute
            uint16_t target_addr = 0x2000 + test.operand;
            memory[0x1001] = target_addr & 0xFF;         // Low byte
            memory[0x1002] = (target_addr >> 8) & 0xFF;  // High byte
            memory[target_addr] = test.initial_value;
        }
    }
    
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    int cycles = 0;
    
    do {
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
    } while (cpu.get_cycle_step() != 0 && cycles < 20);
    
    // Find the result value
    uint8_t result_value = 0;
    if (test.opcode == 0x16 || test.opcode == 0x56 || test.opcode == 0x36 || test.opcode == 0x76) {
        uint8_t target_addr = (test.operand + 0x05) & 0xFF;
        result_value = memory[target_addr];
    } else if (test.opcode == 0x1E || test.opcode == 0x5E || test.opcode == 0x3E || test.opcode == 0x7E) {
        uint16_t base_addr = 0x2000 + test.operand;
        uint16_t target_addr = base_addr + 0x05;
        result_value = memory[target_addr];
    } else if (test.opcode == 0x06 || test.opcode == 0x46 || test.opcode == 0x26 || test.opcode == 0x66) {
        result_value = memory[test.operand];
    } else {
        uint16_t target_addr = 0x2000 + test.operand;
        result_value = memory[target_addr];
    }
    
    bool result_correct = (result_value == test.expected_result);
    bool carry_correct = !!(cpu.get_p() & 0x01) == test.expected_carry;
    bool negative_correct = !!(cpu.get_p() & 0x80) == test.expected_negative;
    bool cycles_correct = (cycles == test.expected_cycles);
    
    std::cout << test.name << " (0x" << std::hex << std::setw(2) << std::setfill('0') << (int)test.opcode << "): ";
    if (result_correct && carry_correct && negative_correct && cycles_correct) {
        std::cout << "✅ PASS" << std::endl;
        return true;
    } else {
        std::cout << "❌ FAIL ";
        if (!result_correct) std::cout << "(result: got 0x" << std::hex << (int)result_value << ", expected 0x" << (int)test.expected_result << ") ";
        if (!carry_correct) std::cout << "(carry: got " << !!(cpu.get_p() & 0x01) << ", expected " << test.expected_carry << ") ";
        if (!negative_correct) std::cout << "(negative: got " << !!(cpu.get_p() & 0x80) << ", expected " << test.expected_negative << ") ";
        if (!cycles_correct) std::cout << "(cycles: got " << std::dec << cycles << ", expected " << test.expected_cycles << ") ";
        std::cout << std::endl;
        return false;
    }
}

int main() {
    std::cout << "=== Testing All Memory Shift/Rotate Operations (Priority 5) ===" << std::endl;
    
    std::vector<TestCase> tests = {
        // ASL - Arithmetic Shift Left
        {0x06, "ASL $nn",     0x10, 0x42, 0x84, false, true,  6},  // Zero page
        {0x16, "ASL $nn,X",   0x10, 0x42, 0x84, false, true,  7},  // Zero page,X
        {0x0E, "ASL $nnnn",   0x10, 0x42, 0x84, false, true,  7},  // Absolute
        {0x1E, "ASL $nnnn,X", 0x10, 0x42, 0x84, false, true,  8},  // Absolute,X
        
        // LSR - Logical Shift Right
        {0x46, "LSR $nn",     0x10, 0x85, 0x42, true,  false, 6},  // Zero page
        {0x56, "LSR $nn,X",   0x10, 0x85, 0x42, true,  false, 7},  // Zero page,X
        {0x4E, "LSR $nnnn",   0x10, 0x85, 0x42, true,  false, 7},  // Absolute
        {0x5E, "LSR $nnnn,X", 0x10, 0x85, 0x42, true,  false, 8},  // Absolute,X
        
        // ROL - Rotate Left (carry clear initially)
        {0x26, "ROL $nn",     0x10, 0x42, 0x84, false, true,  6},  // Zero page
        {0x36, "ROL $nn,X",   0x10, 0x42, 0x84, false, true,  7},  // Zero page,X
        
        // ROR - Rotate Right (carry clear initially)
        {0x66, "ROR $nn",     0x10, 0x85, 0x42, true,  false, 6},  // Zero page
        {0x76, "ROR $nn,X",   0x10, 0x85, 0x42, true,  false, 7},  // Zero page,X
    };
    
    int passed = 0;
    int total = tests.size();
    
    for (const auto& test : tests) {
        if (test_instruction(test)) {
            passed++;
        }
    }
    
    std::cout << "\n=== Priority 5 Results ===" << std::endl;
    std::cout << "Passed: " << passed << "/" << total << std::endl;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(1) << (100.0 * passed / total) << "%" << std::endl;
    
    if (passed == total) {
        std::cout << "\n🎉 PRIORITY 5 COMPLETED! All Memory Shift/Rotate Operations working!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some operations still need fixes" << std::endl;
        return 1;
    }
}