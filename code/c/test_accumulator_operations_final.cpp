#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

int main() {
    std::cout << "=== Priority 4: Accumulator Operations Final Validation ===" << std::endl;
    
    struct AccumulatorTest {
        uint8_t opcode;
        const char* name;
        uint8_t input_a;
        uint8_t input_p;
        uint8_t expected_a;
        uint8_t expected_p;
    };
    
    // Test cases covering different flag combinations
    AccumulatorTest tests[] = {
        // ASL A tests
        {0x0A, "ASL A", 0x42, 0x00, 0x84, 0x80},  // Basic shift left, sets N flag
        {0x0A, "ASL A", 0x80, 0x00, 0x00, 0x03},  // Shift sets C and Z flags
        {0x0A, "ASL A", 0x40, 0x00, 0x80, 0x80},  // Shift sets N flag
        {0x0A, "ASL A", 0x01, 0x00, 0x02, 0x00},  // Simple shift, no flags
        
        // LSR A tests  
        {0x4A, "LSR A", 0x81, 0x00, 0x40, 0x01},  // Shift right sets C flag
        {0x4A, "LSR A", 0x01, 0x00, 0x00, 0x03},  // Shift sets C and Z flags
        {0x4A, "LSR A", 0x80, 0x00, 0x40, 0x00},  // Clear N flag
        {0x4A, "LSR A", 0x02, 0x00, 0x01, 0x00},  // Simple shift, no flags
        
        // ROL A tests
        {0x2A, "ROL A", 0x80, 0x01, 0x01, 0x01},  // Rotate with carry in
        {0x2A, "ROL A", 0x80, 0x00, 0x00, 0x03},  // Rotate sets C and Z flags
        {0x2A, "ROL A", 0x40, 0x00, 0x80, 0x80},  // Rotate sets N flag
        {0x2A, "ROL A", 0x01, 0x01, 0x03, 0x00},  // Rotate with carry, no flags
        
        // ROR A tests
        {0x6A, "ROR A", 0x01, 0x01, 0x80, 0x81},  // Rotate with carry in, sets N and C
        {0x6A, "ROR A", 0x01, 0x00, 0x00, 0x03},  // Rotate sets C and Z flags
        {0x6A, "ROR A", 0x02, 0x01, 0x81, 0x80},  // Rotate with carry sets N flag
        {0x6A, "ROR A", 0x02, 0x00, 0x01, 0x00},  // Simple rotate, no flags
    };
    
    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);
    
    for (const auto& test : tests) {
        TestCPU cpu;
        cpu.init_for_test();
        
        // Set up initial state
        cpu.set_a(test.input_a);
        cpu.set_p(test.input_p);
        cpu.set_pc(0x1000);
        
        // Create memory with the instruction
        std::array<uint8_t, 65536> memory = {};
        memory[0x1000] = test.opcode;
        
        // Execute instruction
        bus_state_t bus_state = 0;
        int cycles = 0;
        
        do {
            uint16_t addr = cpu.get_address();
            if (cpu.get_rw()) {
                // Read operation
                bus_state = BUS_SET_DATA(bus_state, memory[addr]);
            } else {
                // Write operation
                memory[addr] = BUS_GET_DATA(bus_state);
            }
            
            bus_state = cpu.cycle_tick(bus_state);
            cycles++;
        } while (cpu.get_cycle_step() != 0 && cycles < 10);
        
        // Check results
        bool passed_test = (cpu.get_a() == test.expected_a && cpu.get_p() == test.expected_p);
        
        std::cout << std::hex << std::uppercase << std::setfill('0');
        std::cout << test.name << " (0x" << std::setw(2) << (int)test.opcode << "): ";
        std::cout << "A=0x" << std::setw(2) << (int)test.input_a << "→0x" << std::setw(2) << (int)cpu.get_a();
        std::cout << " P=0x" << std::setw(2) << (int)test.input_p << "→0x" << std::setw(2) << (int)cpu.get_p();
        std::cout << " Expected: A=0x" << std::setw(2) << (int)test.expected_a << " P=0x" << std::setw(2) << (int)test.expected_p;
        std::cout << " [" << (passed_test ? "PASS" : "FAIL") << "]" << std::endl;
        
        if (passed_test) {
            passed++;
        } else {
            std::cout << "  *** MISMATCH: Got A=0x" << std::setw(2) << (int)cpu.get_a() 
                      << " P=0x" << std::setw(2) << (int)cpu.get_p() << std::endl;
        }
    }
    
    std::cout << std::dec << std::endl;
    std::cout << "Results: " << passed << "/" << total << " tests passed";
    if (passed == total) {
        std::cout << " ✅ ALL ACCUMULATOR OPERATIONS WORKING!" << std::endl;
    } else {
        std::cout << " ❌ " << (total - passed) << " tests failed" << std::endl;
    }
    
    return (passed == total) ? 0 : 1;
}