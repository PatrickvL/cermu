#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cassert>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace std;

class SBCTestHarness {
private:
    fam65xx_cpp::fam65xx<config_6502>* cpu;
    uint8_t memory[65536];

public:
    SBCTestHarness() {
        cpu = new fam65xx_cpp::fam65xx<config_6502>();
        // Initialize memory to zero
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
    }
    
    ~SBCTestHarness() {
        delete cpu;
    }

    void write_memory(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
    
    uint8_t read_memory(uint16_t addr) {
        return memory[addr];
    }

    void test_sbc_immediate(uint8_t a_initial, uint8_t operand, uint8_t p_initial,
                           uint8_t expected_a, uint8_t expected_p, const string& description) {
        cout << "Test: " << description << endl;
        
        // Setup test
        cpu->init_for_test();
        cpu->set_a(a_initial);
        cpu->set_p(p_initial);
        cpu->set_pc(0x1000);
        
        // Set up memory for SBC immediate: E9 <operand>
        write_memory(0x1000, 0xE9);  // SBC immediate
        write_memory(0x1001, operand);
        
        cout << "  Initial: A=0x" << hex << setw(2) << setfill('0') << (int)a_initial
             << " P=0x" << hex << setw(2) << setfill('0') << (int)p_initial
             << " Operand=0x" << hex << setw(2) << setfill('0') << (int)operand << endl;
        
        // Execute instruction cycle by cycle
        bus_state_t bus = 0;
        int cycles = 0;
        
        while (cpu->get_cycle_step() != 0 || cycles == 0) {
            BUS_SET_ADDR(bus, cpu->get_address());
            BUS_SET_DATA(bus, read_memory(cpu->get_address()));
            bus = cpu->cycle_tick(bus);
            cycles++;
            if (cycles > 10) break; // Safety net
        }
        
        // Check results
        uint8_t result_a = cpu->get_a();
        uint8_t result_p = cpu->get_p() & (P_NEGATIVE | P_ZERO | P_CARRY | P_OVERFLOW);
        uint8_t expected_p_masked = expected_p & (P_NEGATIVE | P_ZERO | P_CARRY | P_OVERFLOW);
        
        cout << "  Final:    A=0x" << hex << setw(2) << setfill('0') << (int)result_a
             << " P=0x" << hex << setw(2) << setfill('0') << (int)result_p << endl;
        cout << "  Expected: A=0x" << hex << setw(2) << setfill('0') << (int)expected_a
             << " P=0x" << hex << setw(2) << setfill('0') << (int)expected_p_masked << endl;
        
        bool correct = (result_a == expected_a && result_p == expected_p_masked);
        cout << "  Result: " << (correct ? "✅ PASS" : "❌ FAIL") << endl << endl;
    }
};

int main() {
    cout << "=== SBC Immediate Mode Test ===" << endl;
    
    SBCTestHarness test;
    
    // Basic subtraction without borrow
    test.test_sbc_immediate(0x50, 0x10, P_CARRY, 0x40, P_CARRY, "Basic: 0x50 - 0x10 = 0x40 (C=1)");
    test.test_sbc_immediate(0x50, 0x10, 0x00, 0x3F, P_CARRY, "With borrow: 0x50 - 0x10 - 1 = 0x3F (C=1)");
    
    // Zero result
    test.test_sbc_immediate(0x30, 0x30, P_CARRY, 0x00, P_CARRY | P_ZERO, "Zero result: 0x30 - 0x30 = 0x00");
    
    // Negative result
    test.test_sbc_immediate(0x10, 0x20, P_CARRY, 0xF0, P_NEGATIVE, "Negative: 0x10 - 0x20 = 0xF0");
    
    // Overflow cases
    test.test_sbc_immediate(0x80, 0x01, P_CARRY, 0x7F, P_CARRY | P_OVERFLOW, "Overflow: 0x80 - 0x01 = 0x7F (V=1)");
    test.test_sbc_immediate(0x7F, 0xFF, P_CARRY, 0x80, P_NEGATIVE | P_OVERFLOW, "Overflow: 0x7F - 0xFF = 0x80 (V=1)");
    
    cout << "=== SBC Tests Complete ===" << endl;
    
    return 0;
}