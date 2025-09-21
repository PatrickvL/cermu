#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

// Test the NMOS 6502 page boundary bug in JMP indirect
int main() {
    std::cout << "=== JMP INDIRECT PAGE BOUNDARY BUG TEST ===" << std::endl;
    
    // Test 1: NMOS 6502 (has bug)
    std::cout << "\n--- TEST 1: NMOS 6502 (page boundary bug) ---" << std::endl;
    {
        fam65xx<config_6502> cpu;
        cpu.init_for_test();
        
        // Set up memory simulation - JMP ($10FF) case (page boundary)
        // Memory layout:
        // $10FF = 0x34 (target low byte)
        // $1100 = 0x12 (target high byte - SHOULD be read)
        // $1000 = 0x56 (wrong high byte - WILL be read due to bug)
        
        bus_state_t bus_state = 0;
        bus_state |= BUS_BIT(BUS_RDY_BIT); // Set RDY line HIGH
        
        // Set up initial state
        cpu.set_pc(0x2000);  // PC starts at $2000
        
        // Cycle 0: Opcode fetch JMP ($nnnn) = 0x6C
        bus_state = BUS_SET_DATA(bus_state, 0x6C);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 1: Read low byte of indirect address = 0xFF
        bus_state = BUS_SET_DATA(bus_state, 0xFF);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 2: Read high byte of indirect address = 0x10  
        bus_state = BUS_SET_DATA(bus_state, 0x10);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 3: Read target low byte from $10FF = 0x34
        std::cout << "Cycle 3: Reading target low byte from address 0x" << std::hex << cpu.get_address() << std::endl;
        bus_state = BUS_SET_DATA(bus_state, 0x34);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 4: Read target high byte - should be from $1000 due to NMOS bug
        uint16_t high_byte_addr = cpu.get_address();
        std::cout << "Cycle 4: Reading target high byte from address 0x" << std::hex << high_byte_addr;
        if (high_byte_addr == 0x1000) {
            std::cout << " (NMOS BUG: reading from $1000 instead of $1100)" << std::endl;
            bus_state = BUS_SET_DATA(bus_state, 0x56); // Wrong high byte due to bug
        } else if (high_byte_addr == 0x1100) {
            std::cout << " (CORRECT: should read from $1100)" << std::endl;
            bus_state = BUS_SET_DATA(bus_state, 0x12); // Correct high byte
        } else {
            std::cout << " (UNEXPECTED ADDRESS!)" << std::endl;
            bus_state = BUS_SET_DATA(bus_state, 0x00);
        }
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 5: Execute the jump
        bus_state = BUS_SET_DATA(bus_state, 0x00); // Dummy data
        bus_state = cpu.cycle_tick(bus_state);
        
        uint16_t final_pc = cpu.get_pc();
        std::cout << "Final PC: 0x" << std::hex << final_pc;
        if (final_pc == 0x5634) {
            std::cout << " (NMOS BUG: jumped to $5634 instead of $1234)" << std::endl;
        } else if (final_pc == 0x1234) {
            std::cout << " (CORRECT: jumped to $1234)" << std::endl;
        } else {
            std::cout << " (UNEXPECTED PC!)" << std::endl;
        }
    }
    
    // Test 2: Normal case (no page boundary)
    std::cout << "\n--- TEST 2: NMOS 6502 (normal case, no page boundary) ---" << std::endl;
    {
        fam65xx<config_6502> cpu;
        cpu.init_for_test();
        
        bus_state_t bus_state = 0;
        bus_state |= BUS_BIT(BUS_RDY_BIT); // Set RDY line HIGH
        
        // Set up initial state
        cpu.set_pc(0x2000);  // PC starts at $2000
        
        // Cycle 0: Opcode fetch JMP ($nnnn) = 0x6C
        bus_state = BUS_SET_DATA(bus_state, 0x6C);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 1: Read low byte of indirect address = 0xFE (NOT page boundary)
        bus_state = BUS_SET_DATA(bus_state, 0xFE);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 2: Read high byte of indirect address = 0x10  
        bus_state = BUS_SET_DATA(bus_state, 0x10);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 3: Read target low byte from $10FE = 0x34
        std::cout << "Cycle 3: Reading target low byte from address 0x" << std::hex << cpu.get_address() << std::endl;
        bus_state = BUS_SET_DATA(bus_state, 0x34);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 4: Read target high byte from $10FF (normal increment)
        uint16_t high_byte_addr = cpu.get_address();
        std::cout << "Cycle 4: Reading target high byte from address 0x" << std::hex << high_byte_addr;
        if (high_byte_addr == 0x10FF) {
            std::cout << " (CORRECT: normal increment to $10FF)" << std::endl;
            bus_state = BUS_SET_DATA(bus_state, 0x12); // Correct high byte
        } else {
            std::cout << " (UNEXPECTED ADDRESS!)" << std::endl;
            bus_state = BUS_SET_DATA(bus_state, 0x00);
        }
        bus_state = cpu.cycle_tick(bus_state);
        
        // Cycle 5: Execute the jump
        bus_state = BUS_SET_DATA(bus_state, 0x00); // Dummy data
        bus_state = cpu.cycle_tick(bus_state);
        
        uint16_t final_pc = cpu.get_pc();
        std::cout << "Final PC: 0x" << std::hex << final_pc;
        if (final_pc == 0x1234) {
            std::cout << " (CORRECT: jumped to $1234)" << std::endl;
        } else {
            std::cout << " (UNEXPECTED PC!)" << std::endl;
        }
    }
    
    return 0;
}