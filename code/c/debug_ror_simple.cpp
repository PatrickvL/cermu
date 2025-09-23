#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

int main() {
    std::cout << "=== Simple ROR Flag Test ===\n";
    
    TestCPU cpu;
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    cpu.set_p(0x24); // Clear carry flag, set other flags as in test
    
    std::cout << "Initial P register: 0x" << std::hex << (int)cpu.get_p() << "\n";
    std::cout << "Initial carry flag: " << ((cpu.get_p() & 0x01) ? 1 : 0) << "\n";
    
    std::array<uint8_t, 65536> memory = {};
    memory[0x1000] = 0x66; // ROR $nn
    memory[0x1001] = 0x10; // Zero page address
    memory[0x0010] = 0x85; // Value to rotate: 0x85
    
    std::cout << "Input value: 0x" << std::hex << (int)memory[0x0010] << " (binary: ";
    for (int i = 7; i >= 0; i--) {
        std::cout << ((memory[0x0010] >> i) & 1);
    }
    std::cout << ")\n";
    
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
        
        std::cout << "After cycle " << cycles << ": P = 0x" << std::hex << (int)cpu.get_p() 
                  << ", carry = " << ((cpu.get_p() & 0x01) ? 1 : 0) 
                  << ", mem[0x10] = 0x" << std::hex << (int)memory[0x0010] << "\n";
        
    } while (cpu.get_cycle_step() != 0 && cycles < 10);
    
    uint8_t result = memory[0x0010];
    bool carry = !!(cpu.get_p() & 0x01);
    bool negative = !!(cpu.get_p() & 0x80);
    bool zero = !!(cpu.get_p() & 0x02);
    
    std::cout << "\nFinal result: 0x" << std::hex << (int)result << " (binary: ";
    for (int i = 7; i >= 0; i--) {
        std::cout << ((result >> i) & 1);
    }
    std::cout << ")\n";
    
    std::cout << "Final P register: 0x" << std::hex << (int)cpu.get_p() << "\n";
    std::cout << "Carry flag: " << carry << " (expected: 1)\n";
    std::cout << "Negative flag: " << negative << " (expected: 0)\n";
    std::cout << "Zero flag: " << zero << " (expected: 0)\n";
    
    // Expected: 0x85 >> 1 = 0x42, carry = 1 (from bit 0), negative = 0
    bool result_correct = (result == 0x42);
    bool carry_correct = (carry == true);
    bool negative_correct = (negative == false);
    
    std::cout << "\nTest results:\n";
    std::cout << "Result: " << (result_correct ? "✅ PASS" : "❌ FAIL") << "\n";
    std::cout << "Carry: " << (carry_correct ? "✅ PASS" : "❌ FAIL") << "\n";
    std::cout << "Negative: " << (negative_correct ? "✅ PASS" : "❌ FAIL") << "\n";
    
    return (result_correct && carry_correct && negative_correct) ? 0 : 1;
}