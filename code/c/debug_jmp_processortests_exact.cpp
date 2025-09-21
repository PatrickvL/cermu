#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>

using namespace fam65xx_cpp;

// Simple memory for testing
uint8_t test_memory[0x10000];

// Simple bus interface that exactly matches ProcessorTests
bus_state_t read_memory(uint16_t addr) {
    bus_state_t result = 0;
    return BUS_SET_DATA(result, test_memory[addr]);
}

void write_memory(uint16_t addr, uint8_t data) {
    test_memory[addr] = data;
}

// Debug function to show processor flags
void show_flags(uint8_t p, const char* label) {
    std::cout << label << " P=$" << std::hex << std::setfill('0') << std::setw(2) << (int)p;
    std::cout << " (N=" << ((p & 0x80) ? 1 : 0);
    std::cout << " V=" << ((p & 0x40) ? 1 : 0);
    std::cout << " U=" << ((p & 0x20) ? 1 : 0);
    std::cout << " B=" << ((p & 0x10) ? 1 : 0);
    std::cout << " D=" << ((p & 0x08) ? 1 : 0);
    std::cout << " I=" << ((p & 0x04) ? 1 : 0);
    std::cout << " Z=" << ((p & 0x02) ? 1 : 0);
    std::cout << " C=" << ((p & 0x01) ? 1 : 0);
    std::cout << ")" << std::endl;
}

// Test a specific case that fails in ProcessorTests
int main() {
    std::cout << "=== JMP ProcessorTests Exact Reproduction Test ===" << std::endl;
    
    // Create CPU instance using exact same config as ProcessorTests
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Try one of the failing cases from verbose output: "4c 2a cd"
    // This means JMP $CD2A with some specific initial state
    
    // Set up memory: JMP $CD2A instruction
    test_memory[0x3b72] = 0x4C;  // JMP absolute
    test_memory[0x3b73] = 0x2A;  // Low byte of target
    test_memory[0x3b74] = 0xCD;  // High byte of target
    
    // Set PC to start of instruction (this PC was from ProcessorTests output)
    cpu.set_pc(0x3b72);
    
    // Set initial P register that was expected: 0x26
    // This means: N=0, V=0, U=1, B=0, D=0, I=1, Z=1, C=0
    cpu.set_p(0x26);
    
    std::cout << "=== INITIAL STATE (Exact ProcessorTests reproduction) ===" << std::endl;
    std::cout << "PC: $" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    show_flags(cpu.get_p(), "Initial");
    
    // Execute JMP instruction cycle by cycle, exactly like ProcessorTests
    bus_state_t bus_state = 0;
    
    std::cout << "\n=== CYCLE EXECUTION ===" << std::endl;
    for (int cycle = 0; cycle < 5; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        std::cout << "Before cycle " << cycle << ":" << std::endl;
        std::cout << "  addr=$" << std::hex << std::setfill('0') << std::setw(4) << addr;
        std::cout << " " << (is_write ? "W" : "R");
        show_flags(cpu.get_p(), "  Pre-cycle");
        
        if (is_write) {
            uint8_t data = cpu.get_write_data();
            write_memory(addr, data);
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            uint8_t data = test_memory[addr];
            bus_state = BUS_SET_DATA(bus_state, data);
        }
        
        // CRITICAL: Set control pins exactly like ProcessorTests
        bus_state |= BUS_BIT(BUS_RDY_BIT);   // RDY line (ready)
        bus_state |= BUS_BIT(BUS_SO_BIT);    // SO pin inactive (high)
        
        std::cout << "  data=$" << std::hex << std::setfill('0') << std::setw(2) << (int)BUS_GET_DATA(bus_state) << std::endl;
        
        // Execute the cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "After cycle " << cycle << ":" << std::endl;
        show_flags(cpu.get_p(), "  Post-cycle");
        std::cout << "  PC=$" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
        
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0) {
            std::cout << "Instruction complete after cycle " << cycle << std::endl;
            break;
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n=== FINAL STATE ===" << std::endl;
    std::cout << "PC: $" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::endl;
    show_flags(cpu.get_p(), "Final");
    
    // Check if we match expected result
    uint8_t final_p = cpu.get_p();
    uint8_t expected_p = 0x26;  // Should remain unchanged
    
    if (final_p == expected_p) {
        std::cout << "✅ SUCCESS: P register matches expected value" << std::endl;
        return 0;
    } else {
        std::cout << "❌ FAILURE: P register mismatch!" << std::endl;
        std::cout << "Expected P: $" << std::hex << (int)expected_p << std::endl;
        std::cout << "Actual P:   $" << std::hex << (int)final_p << std::endl;
        std::cout << "Difference: $" << std::hex << (int)(final_p ^ expected_p) << std::endl;
        
        if ((final_p ^ expected_p) == 0x40) {
            std::cout << ">>> CONFIRMED: V flag incorrectly set (difference = 0x40)" << std::endl;
        }
        return 1;
    }
}