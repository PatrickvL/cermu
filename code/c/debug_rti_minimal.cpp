#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== RTI Minimal Debug Test ===" << std::endl;
    
    // Create CPU with NMOS 6502 configuration
    config_6502 config;
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state similar to ProcessorTests
    cpu.set_pc(0x8000);
    cpu.set_sp(0xFC);  // Stack pointer before RTI (will be incremented)
    cpu.set_p(0x24);   // Initial status register
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_sp() << std::endl;
    std::cout << "  P:  0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    
    // Execute cycles manually with simple memory responses
    uint8_t memory[65536] = {0};
    
    // Set up memory
    memory[0x8000] = 0x40;  // RTI instruction
    memory[0x01FD] = 0x30;  // Status register to restore
    memory[0x01FE] = 0x34;  // PC low byte
    memory[0x01FF] = 0x12;  // PC high byte
    
    std::cout << "\nStack setup:" << std::endl;
    std::cout << "  [0x01FD] = 0x30 (Status)" << std::endl;
    std::cout << "  [0x01FE] = 0x34 (PC Low)" << std::endl;
    std::cout << "  [0x01FF] = 0x12 (PC High)" << std::endl;
    
    int cycle_count = 0;
    bool instruction_complete = false;
    
    std::cout << "\nExecuting RTI:" << std::endl;
    
    while (!instruction_complete && cycle_count < 10) {
        // Get the address that the CPU wants to access
        uint16_t address = cpu.get_address();
        bool is_write = !cpu.get_rw();
        uint8_t write_data = cpu.get_write_data();
        
        // Provide memory data for reads
        uint8_t read_data = memory[address];
        
        std::cout << "Cycle " << cycle_count << ": ";
        std::cout << "Addr=0x" << std::hex << std::setw(4) << std::setfill('0') << address;
        
        if (is_write) {
            std::cout << " WRITE 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)write_data;
            memory[address] = write_data;
        } else {
            std::cout << " READ 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)read_data;
        }
        
        std::cout << " (step=" << (int)cpu.get_cycle_step() << ")";
        
        // Create bus state with the memory data
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_DATA(bus_state, read_data);
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // Always ready
        
        // Execute the cycle
        uint8_t prev_step = cpu.get_cycle_step();
        bus_state = cpu.cycle_tick(bus_state);
        uint8_t curr_step = cpu.get_cycle_step();
        
        std::cout << " → step=" << (int)curr_step;
        std::cout << std::endl;
        
        cycle_count++;
        
        // Check if instruction completed (step went to 0)
        if (prev_step > 0 && curr_step == 0) {
            instruction_complete = true;
            std::cout << "*** INSTRUCTION COMPLETE ***" << std::endl;
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_sp() << std::endl;
    std::cout << "  P:  0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    std::cout << "  Total cycles: " << cycle_count << std::endl;
    
    // Check expected results
    bool pc_correct = (cpu.get_pc() == 0x1234);
    bool p_correct = (cpu.get_p() == 0x30);
    bool sp_correct = (cpu.get_sp() == 0xFF);  // SP should be incremented 3 times
    bool cycles_correct = (cycle_count == 6);
    
    std::cout << "\nValidation:" << std::endl;
    std::cout << "  PC correct (0x1234): " << (pc_correct ? "✓" : "✗") << std::endl;
    std::cout << "  P correct (0x30):    " << (p_correct ? "✓" : "✗") << std::endl;
    std::cout << "  SP correct (0xFF):   " << (sp_correct ? "✓" : "✗") << std::endl;
    std::cout << "  Cycles correct (6):  " << (cycles_correct ? "✓" : "✗") << std::endl;
    
    if (pc_correct && p_correct && sp_correct && cycles_correct) {
        std::cout << "\n✓ RTI working correctly!" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ RTI has issues" << std::endl;
        return 1;
    }
}