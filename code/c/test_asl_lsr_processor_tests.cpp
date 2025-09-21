// Test ASL A and LSR A against ProcessorTests to verify our fix
#include <iostream>
#include <iomanip>
#include <vector>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

bool test_asl_a() {
    auto cpu = fam65xx<config_6502>();
    cpu.init_for_test();
    
    // Test case: A=0x42, P=0x00, PC=0x0000
    // Expected: A=0x84, P=0x80 (N flag), PC=0x0001
    cpu.set_pc(0x0000);
    cpu.set_a(0x42);
    cpu.set_p(0x00);
    
    // Create a simple memory simulation
    std::vector<uint8_t> memory(65536, 0);
    memory[0x0000] = 0x0A; // ASL A instruction
    
    // Execute ASL A instruction (2 cycles)
    for (int cycle = 0; cycle < 2; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        uint8_t data = 0;
        
        if (is_read) {
            data = memory[addr];
        } else {
            data = cpu.get_write_data();
            memory[addr] = data;
        }
        
        // Create bus state with data
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state |= BUS_BIT(BUS_RDY_BIT); // RDY high (ready)
        
        cpu.cycle_tick(bus_state);
    }
    
    // Check results
    bool pc_match = (cpu.get_pc() == 0x0001);
    bool a_match = (cpu.get_a() == 0x84);
    bool p_match = (cpu.get_p() == 0x80); // N flag set
    
    std::cout << "ASL A Test:" << std::endl;
    std::cout << "Expected: PC=0x0001 A=0x84 P=0x80" << std::endl;
    std::cout << "Got:      PC=0x" << std::hex << cpu.get_pc() 
              << " A=0x" << cpu.get_a() 
              << " P=0x" << cpu.get_p() << std::endl;
    
    return pc_match && a_match && p_match;
}

bool test_asl_a_carry() {
    auto cpu = fam65xx<config_6502>();
    cpu.init_for_test();
    
    // Test case: A=0x80, P=0x00, PC=0x0000
    // Expected: A=0x00, P=0x03 (Z flag + C flag), PC=0x0001
    cpu.set_pc(0x0000);
    cpu.set_a(0x80);  // High bit set
    cpu.set_p(0x00);
    
    // Create a simple memory simulation
    std::vector<uint8_t> memory(65536, 0);
    memory[0x0000] = 0x0A; // ASL A instruction
    
    // Execute ASL A instruction (2 cycles)
    for (int cycle = 0; cycle < 2; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        uint8_t data = 0;
        
        if (is_read) {
            data = memory[addr];
        } else {
            data = cpu.get_write_data();
            memory[addr] = data;
        }
        
        // Create bus state with data
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state |= BUS_BIT(BUS_RDY_BIT); // RDY high (ready)
        
        cpu.cycle_tick(bus_state);
    }
    
    // Check results
    bool pc_match = (cpu.get_pc() == 0x0001);
    bool a_match = (cpu.get_a() == 0x00);
    bool p_match = (cpu.get_p() == 0x03); // Z flag (0x02) + C flag (0x01)
    
    std::cout << "ASL A Carry Test:" << std::endl;
    std::cout << "Expected: PC=0x0001 A=0x00 P=0x03" << std::endl;
    std::cout << "Got:      PC=0x" << std::hex << cpu.get_pc() 
              << " A=0x" << cpu.get_a() 
              << " P=0x" << cpu.get_p() << std::endl;
    
    return pc_match && a_match && p_match;
}

bool test_lsr_a() {
    auto cpu = fam65xx<config_6502>();
    cpu.init_for_test();
    
    // Test case: A=0x85, P=0x00, PC=0x0000
    // Expected: A=0x42, P=0x01 (C flag), PC=0x0001
    cpu.set_pc(0x0000);
    cpu.set_a(0x85);  // Low bit set
    cpu.set_p(0x00);
    
    // Create a simple memory simulation
    std::vector<uint8_t> memory(65536, 0);
    memory[0x0000] = 0x4A; // LSR A instruction
    
    // Execute LSR A instruction (2 cycles)
    for (int cycle = 0; cycle < 2; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        uint8_t data = 0;
        
        if (is_read) {
            data = memory[addr];
        } else {
            data = cpu.get_write_data();
            memory[addr] = data;
        }
        
        // Create bus state with data
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state |= BUS_BIT(BUS_RDY_BIT); // RDY high (ready)
        
        cpu.cycle_tick(bus_state);
    }
    
    // Check results
    bool pc_match = (cpu.get_pc() == 0x0001);
    bool a_match = (cpu.get_a() == 0x42);
    bool p_match = (cpu.get_p() == 0x01); // C flag set
    
    std::cout << "LSR A Test:" << std::endl;
    std::cout << "Expected: PC=0x0001 A=0x42 P=0x01" << std::endl;
    std::cout << "Got:      PC=0x" << std::hex << cpu.get_pc() 
              << " A=0x" << cpu.get_a() 
              << " P=0x" << cpu.get_p() << std::endl;
    
    return pc_match && a_match && p_match;
}

int main() {
    std::cout << "=== Testing Accumulator Operations Against ProcessorTests Pattern ===" << std::endl;
    
    bool test1 = test_asl_a();
    std::cout << "Result: " << (test1 ? "PASS" : "FAIL") << std::endl << std::endl;
    
    bool test2 = test_asl_a_carry();
    std::cout << "Result: " << (test2 ? "PASS" : "FAIL") << std::endl << std::endl;
    
    bool test3 = test_lsr_a();
    std::cout << "Result: " << (test3 ? "PASS" : "FAIL") << std::endl << std::endl;
    
    bool all_passed = test1 && test2 && test3;
    
    if (all_passed) {
        std::cout << "🎉 PRIORITY 4 COMPLETE: Accumulator Operations Fixed!" << std::endl;
        std::cout << "✅ ASL A (0x0A) - Working with proper flag handling" << std::endl;
        std::cout << "✅ LSR A (0x4A) - Working with proper flag handling" << std::endl;
        std::cout << "✅ ROL A (0x2A) - Working (verified by ALU test)" << std::endl;
        std::cout << "✅ ROR A (0x6A) - Working (verified by ALU test)" << std::endl;
        std::cout << std::endl;
        std::cout << "Key Fix: Created accumulator-specific ALU operations (ASL_ACC, LSR_ACC, ROL_ACC, ROR_ACC)" << std::endl;
        std::cout << "that directly modify the accumulator register instead of storing results in DL register." << std::endl;
    } else {
        std::cout << "❌ Some accumulator operations are still failing" << std::endl;
    }
    
    return all_passed ? 0 : 1;
}