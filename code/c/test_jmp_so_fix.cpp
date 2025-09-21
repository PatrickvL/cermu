#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

// Simple memory for testing
uint8_t test_memory[0x10000];

// Simple bus interface
bus_state_t read_memory(uint16_t addr) {
    bus_state_t result = 0;
    return BUS_SET_DATA(result, test_memory[addr]);
}

void write_memory(uint16_t addr, uint8_t data) {
    test_memory[addr] = data;
}

int main() {
    std::cout << "=== JMP SO Fix Test ===" << std::endl;
    
    // Create CPU instance
    fam65xx<config_6510> cpu;
    cpu.init_for_test();
    
    // Set up JMP instruction: JMP $2000
    test_memory[0x1000] = 0x4C;  // JMP absolute
    test_memory[0x1001] = 0x00;  // Low byte of target
    test_memory[0x1002] = 0x20;  // High byte of target
    
    // Set PC to start of instruction
    cpu.set_pc(0x1000);
    
    // Set initial P register with all flags clear except required ones
    cpu.set_p(0x20);  // Only U flag set (required by 6502)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "PC: $" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "P:  $" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_p() << std::endl;
    
    // Execute JMP instruction cycle by cycle
    bus_state_t bus_state = 0;
    
    for (int cycle = 0; cycle < 5; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        if (is_write) {
            uint8_t data = cpu.get_write_data();
            write_memory(addr, data);
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            uint8_t data = test_memory[addr];
            bus_state = BUS_SET_DATA(bus_state, data);
        }
        
        std::cout << "Cycle " << cycle << ": addr=$" << std::hex << std::setfill('0') << std::setw(4) << addr;
        std::cout << " data=$" << std::hex << std::setfill('0') << std::setw(2) << (int)BUS_GET_DATA(bus_state);
        std::cout << " " << (is_write ? "W" : "R") << std::endl;
        
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0) {
            std::cout << "Instruction complete after cycle " << cycle << std::endl;
            break;
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "PC: $" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "P:  $" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_p() << std::endl;
    
    // Check if P register changed (it shouldn't have!)
    uint8_t final_p = cpu.get_p();
    if (final_p == 0x20) {
        std::cout << "✅ SUCCESS: P register unchanged (JMP doesn't affect flags)" << std::endl;
        return 0;
    } else {
        std::cout << "❌ FAILURE: P register changed from $20 to $" << std::hex << (int)final_p << std::endl;
        std::cout << "V flag incorrectly set: " << ((final_p & 0x40) ? "YES" : "NO") << std::endl;
        return 1;
    }
}