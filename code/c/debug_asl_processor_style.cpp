#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== ASL A ProcessorTests-Style Execution ===\n";

    // Use EXACT same CPU type as ProcessorTests runner
    fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536] = {0};
    
    // EXACT same initialization as ProcessorTests
    cpu.init_for_test();
    
    // Set up the EXACT same test state
    cpu.set_pc(0xd91a);
    cpu.set_a(0x39);
    cpu.set_p(0x2a);
    
    // Set up memory with ASL A instruction
    memory[0xd91a] = 0x0a;  // ASL A opcode
    
    std::cout << "=== Initial State ===\n";
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A:  0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "P:  0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "Cycle: " << std::dec << cpu.get_cycle_count() << std::endl;
    
    // Execute instruction using EXACT ProcessorTests pattern
    uint32_t initial_cycles = cpu.get_cycle_count();
    
    for (int max_cycles = 0; max_cycles < 10; max_cycles++) {
        // EXACT ProcessorTests memory interface pattern
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        std::cout << "\n=== CYCLE " << max_cycles << " ===\n";
        std::cout << "PC before: 0x" << std::hex << cpu.get_pc() << std::endl;
        std::cout << "Address: 0x" << std::hex << addr << std::endl;
        std::cout << "Is Write: " << (is_write ? "YES" : "NO") << std::endl;
        
        bus_state_t bus_state = 0;
        if (is_write) {
            // Write cycle
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            BUS_SET_DATA(bus_state, data);
            std::cout << "Write data: 0x" << std::hex << (int)data << std::endl;
        } else {
            // Read cycle
            uint8_t data = memory[addr];
            BUS_SET_DATA(bus_state, data);
            std::cout << "Read data: 0x" << std::hex << (int)data << std::endl;
        }
        
        // Set RDY line (ready)
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "PC after: 0x" << std::hex << cpu.get_pc() << std::endl;
        std::cout << "A after: 0x" << std::hex << (int)cpu.get_a() << std::endl;
        std::cout << "P after: 0x" << std::hex << (int)cpu.get_p() << std::endl;
        std::cout << "Cycle step: " << std::dec << (int)cpu.get_cycle_step() << std::endl;
        
        // Check if instruction completed (cycle_step wrapped back to 0)
        if (cpu.get_cycle_step() == 0 && max_cycles > 0) {
            std::cout << "\n=== INSTRUCTION COMPLETED ===\n";
            break;
        }
    }
    
    uint32_t cycles_executed = cpu.get_cycle_count() - initial_cycles;
    
    std::cout << "\n=== FINAL RESULTS ===\n";
    std::cout << "Expected PC: 0xd91b (0xd91a + 1)\n";
    std::cout << "Actual PC:   0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Expected A:  0x72 (0x39 << 1)\n";
    std::cout << "Actual A:    0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "Expected P:  0x28\n";
    std::cout << "Actual P:    0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "Cycles executed: " << std::dec << cycles_executed << std::endl;
    
    // Check results
    bool pc_correct = (cpu.get_pc() == 0xd91b);
    bool a_correct = (cpu.get_a() == 0x72);
    bool p_correct = (cpu.get_p() == 0x28);
    
    std::cout << "\nPC correct: " << (pc_correct ? "YES" : "NO") << std::endl;
    std::cout << "A correct:  " << (a_correct ? "YES" : "NO") << std::endl;
    std::cout << "P correct:  " << (p_correct ? "YES" : "NO") << std::endl;
    
    if (pc_correct && a_correct && p_correct) {
        std::cout << "*** TEST PASSED ***" << std::endl;
    } else {
        std::cout << "*** TEST FAILED ***" << std::endl;
        if (!pc_correct) {
            std::cout << "PC FAILURE: Extra increment detected!" << std::endl;
        }
        if (!a_correct) {
            std::cout << "A FAILURE: ALU operation not executed!" << std::endl;
        }
        if (!p_correct) {
            std::cout << "P FAILURE: Flags not updated!" << std::endl;
        }
    }
    
    return 0;
}