#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

// Simple test harness to debug a single store instruction
class StoreDebugHarness {
private:
    fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];

public:
    StoreDebugHarness() {
        std::fill(memory, memory + 65536, 0);
        cpu.init_for_test();
    }

    void setup_test(uint16_t pc, uint8_t a_reg, uint16_t target_addr, uint8_t low_byte, uint8_t high_byte) {
        // Clear memory
        std::fill(memory, memory + 65536, 0);
        cpu.init_for_test();
        
        // Set CPU state
        cpu.set_pc(pc);
        cpu.set_a(a_reg);
        
        // Set up STA absolute instruction: 8D low high
        memory[pc] = 0x8D;          // STA absolute opcode
        memory[pc + 1] = low_byte;  // Address low byte
        memory[pc + 2] = high_byte; // Address high byte
        
        std::cout << "=== SETUP ===" << std::endl;
        std::cout << "PC: 0x" << std::hex << std::setfill('0') << std::setw(4) << pc << std::endl;
        std::cout << "A:  0x" << std::hex << std::setfill('0') << std::setw(2) << (int)a_reg << std::endl;
        std::cout << "Target: 0x" << std::hex << std::setfill('0') << std::setw(4) << target_addr << std::endl;
        std::cout << "Instruction: 8D " << std::hex << std::setfill('0') << std::setw(2) << (int)low_byte 
                  << " " << std::setfill('0') << std::setw(2) << (int)high_byte << std::endl;
        std::cout << std::dec << std::endl;
    }

    void execute_with_trace() {
        std::cout << "=== EXECUTION TRACE ===" << std::endl;
        
        uint32_t initial_cycles = cpu.get_cycle_count();
        
        for (int cycle = 1; cycle <= 10; cycle++) {
            uint16_t addr = cpu.get_address();
            bool is_write = !cpu.get_rw();
            uint8_t write_data = cpu.get_write_data();
            
            std::cout << "Cycle " << cycle << ": ";
            std::cout << "Addr=0x" << std::hex << std::setfill('0') << std::setw(4) << addr;
            
            bus_state_t bus_state = 0;
            if (is_write) {
                std::cout << " WRITE data=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)write_data;
                memory[addr] = write_data;
                BUS_SET_DATA(bus_state, write_data);
            } else {
                uint8_t read_data = memory[addr];
                std::cout << " READ data=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)read_data;
                BUS_SET_DATA(bus_state, read_data);
            }
            
            bus_state |= BUS_BIT(BUS_RDY_BIT);
            bus_state = cpu.cycle_tick(bus_state);
            
            std::cout << " PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc();
            std::cout << " step=" << std::dec << cpu.get_cycle_step() << std::endl;
            
            if (cpu.get_cycle_step() == 0 && cycle > 1) {
                std::cout << "Instruction complete after " << cycle << " cycles" << std::endl;
                break;
            }
        }
        
        uint32_t cycles_executed = cpu.get_cycle_count() - initial_cycles;
        std::cout << "Total cycles: " << cycles_executed << std::endl;
        std::cout << std::dec << std::endl;
    }

    void check_result(uint16_t target_addr, uint8_t expected_value) {
        std::cout << "=== RESULT ===" << std::endl;
        std::cout << "Expected at 0x" << std::hex << std::setfill('0') << std::setw(4) << target_addr 
                  << ": 0x" << std::setfill('0') << std::setw(2) << (int)expected_value << std::endl;
        std::cout << "Actual   at 0x" << std::hex << std::setfill('0') << std::setw(4) << target_addr 
                  << ": 0x" << std::setfill('0') << std::setw(2) << (int)memory[target_addr] << std::endl;
        
        bool success = memory[target_addr] == expected_value;
        std::cout << "Result: " << (success ? "PASS" : "FAIL") << std::endl;
        std::cout << std::dec << std::endl;
    }
};

int main() {
    StoreDebugHarness harness;
    
    // Test the successful case "8d 31 50" first
    std::cout << "=== TESTING SUCCESSFUL CASE: STA $5031 ===" << std::endl;
    harness.setup_test(0x1000, 0xAB, 0x5031, 0x31, 0x50);  // STA $5031 with A=0xAB
    harness.execute_with_trace();
    harness.check_result(0x5031, 0xAB);
    
    std::cout << "\n" << std::string(60, '=') << "\n" << std::endl;
    
    // Test a failing case - corrected endianness
    std::cout << "=== TESTING CORRECTED CASE: STA $29F1 ===" << std::endl;
    harness.setup_test(0x1000, 0xCD, 0x29F1, 0xF1, 0x29);  // STA $29F1 with A=0xCD (little-endian)
    harness.execute_with_trace();
    harness.check_result(0x29F1, 0xCD);
    
    return 0;
}