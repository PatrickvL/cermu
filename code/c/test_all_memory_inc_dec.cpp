#include <iostream>
#include <iomanip>
#include <array>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

bool test_inc_dec_operation(uint8_t opcode, const char* name, uint16_t addr, uint8_t initial_value, uint8_t expected_result, uint8_t expected_flags) {
    TestCPU cpu;
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    cpu.set_p(0x24); // Clear all flags except unused
    
    std::array<uint8_t, 65536> memory = {};
    
    // Set up instruction based on opcode
    memory[0x1000] = opcode;
    
    if (opcode == 0xE6 || opcode == 0xC6) {
        // Zero page: INC/DEC $nn
        memory[0x1001] = addr & 0xFF;
        memory[addr] = initial_value;
    } else if (opcode == 0xF6 || opcode == 0xD6) {
        // Zero page,X: INC/DEC $nn,X
        cpu.set_x(0x02); // X register = 2
        memory[0x1001] = (addr - 2) & 0xFF; // Base address
        memory[addr] = initial_value; // Effective address = base + X
    } else if (opcode == 0xEE || opcode == 0xCE) {
        // Absolute: INC/DEC $nnnn
        memory[0x1001] = addr & 0xFF;
        memory[0x1002] = (addr >> 8) & 0xFF;
        memory[addr] = initial_value;
    } else if (opcode == 0xFE || opcode == 0xDE) {
        // Absolute,X: INC/DEC $nnnn,X
        cpu.set_x(0x02); // X register = 2
        memory[0x1001] = (addr - 2) & 0xFF; // Base address low
        memory[0x1002] = ((addr - 2) >> 8) & 0xFF; // Base address high
        memory[addr] = initial_value; // Effective address = base + X
    }
    
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
    
    uint8_t result = memory[addr];
    uint8_t flags = cpu.get_p() & (0x80 | 0x02); // N and Z flags only
    
    bool result_ok = (result == expected_result);
    bool flags_ok = (flags == expected_flags);
    
    std::cout << name << " (0x" << std::hex << std::setfill('0') << std::setw(2) << (int)opcode << "): ";
    if (result_ok && flags_ok) {
        std::cout << "✅ PASS";
    } else {
        std::cout << "❌ FAIL";
        if (!result_ok) {
            std::cout << " (result: got 0x" << std::hex << (int)result 
                      << ", expected 0x" << (int)expected_result << ")";
        }
        if (!flags_ok) {
            std::cout << " (flags: got 0x" << std::hex << (int)flags 
                      << ", expected 0x" << (int)expected_flags << ")";
        }
    }
    std::cout << std::endl;
    
    return result_ok && flags_ok;
}

int main() {
    std::cout << "=== Testing All Memory Inc/Dec Operations (Priority 6) ===" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // Test INC operations
    // INC $nn (0xE6)
    total++; if (test_inc_dec_operation(0xE6, "INC $nn", 0x10, 0x7F, 0x80, 0x80)) passed++; // 0x7F + 1 = 0x80 (negative)
    
    // INC $nn,X (0xF6)  
    total++; if (test_inc_dec_operation(0xF6, "INC $nn,X", 0x12, 0xFE, 0xFF, 0x80)) passed++; // 0xFE + 1 = 0xFF (negative)
    
    // INC $nnnn (0xEE)
    total++; if (test_inc_dec_operation(0xEE, "INC $nnnn", 0x2000, 0xFF, 0x00, 0x02)) passed++; // 0xFF + 1 = 0x00 (zero)
    
    // INC $nnnn,X (0xFE)
    total++; if (test_inc_dec_operation(0xFE, "INC $nnnn,X", 0x2002, 0x42, 0x43, 0x00)) passed++; // 0x42 + 1 = 0x43 (positive)
    
    // Test DEC operations
    // DEC $nn (0xC6)
    total++; if (test_inc_dec_operation(0xC6, "DEC $nn", 0x10, 0x01, 0x00, 0x02)) passed++; // 0x01 - 1 = 0x00 (zero)
    
    // DEC $nn,X (0xD6)
    total++; if (test_inc_dec_operation(0xD6, "DEC $nn,X", 0x12, 0x80, 0x7F, 0x00)) passed++; // 0x80 - 1 = 0x7F (positive)
    
    // DEC $nnnn (0xCE)
    total++; if (test_inc_dec_operation(0xCE, "DEC $nnnn", 0x2000, 0x00, 0xFF, 0x80)) passed++; // 0x00 - 1 = 0xFF (negative)
    
    // DEC $nnnn,X (0xDE)
    total++; if (test_inc_dec_operation(0xDE, "DEC $nnnn,X", 0x2002, 0x43, 0x42, 0x00)) passed++; // 0x43 - 1 = 0x42 (positive)
    
    std::cout << "\n=== Priority 6 Results ===" << std::endl;
    std::cout << "Passed: " << std::hex << passed << "/" << std::hex << total << std::endl;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(1) 
              << (100.0 * passed / total) << "%" << std::endl;
    
    if (passed == total) {
        std::cout << "\n🎉 PRIORITY 6 COMPLETED! All Memory Inc/Dec Operations working!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some operations still need fixes" << std::endl;
        return 1;
    }
}