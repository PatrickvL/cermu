#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== Single ASL $10 Debug Test ===\n";
    
    fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[0x10000];
    std::fill(memory, memory + sizeof(memory), 0xEA);
    
    // Set up ASL $10 instruction
    memory[0x0000] = 0x06;  // ASL $nn
    memory[0x0001] = 0x10;  // Zero page address
    memory[0x0010] = 0x40;  // Test data: 0x40 << 1 should = 0x80
    
    cpu.init_for_test();
    cpu.set_pc(0x0000);
    
    std::cout << "Initial setup: Memory[$0010]=0x40\n";
    std::cout << "Expected result: 0x40 << 1 = 0x80\n\n";
    
    bus_state_t bus_state = 0;
    int cycle_count = 0;
    
    do {
        cycle_count++;
        uint16_t addr = cpu.get_address();
        uint8_t data = memory[addr];
        
        std::cout << "--- Cycle " << cycle_count << " ---\n";
        std::cout << "Address: $" << std::hex << std::setfill('0') << std::setw(4) << addr << std::dec << "\n";
        std::cout << "RW: " << (cpu.get_rw() ? "READ" : "WRITE") << "\n";
        
        if (!cpu.get_rw()) {
            uint8_t write_data = cpu.get_write_data();
            std::cout << "Write data: 0x" << std::hex << std::setw(2) << (int)write_data << std::dec << "\n";
            memory[addr] = write_data;
        } else {
            std::cout << "Read data: 0x" << std::hex << std::setw(2) << (int)data << std::dec << "\n";
        }
        
        // Debug CPU internal state
        std::cout << "CPU State:\n";
        std::cout << "  DL register: 0x" << std::hex << std::setw(2) << (int)cpu.get_reg(static_cast<int>(CpuReg::DL)) << std::dec << "\n";
        std::cout << "  A register: 0x" << std::hex << std::setw(2) << (int)cpu.get_a() << std::dec << "\n";
        std::cout << "  P register: 0x" << std::hex << std::setw(2) << (int)cpu.get_p() << std::dec << "\n";
        std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << "\n";
        
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "After cycle:\n";
        std::cout << "  DL register: 0x" << std::hex << std::setw(2) << (int)cpu.get_reg(static_cast<int>(CpuReg::DL)) << std::dec << "\n";
        std::cout << "  A register: 0x" << std::hex << std::setw(2) << (int)cpu.get_a() << std::dec << "\n";
        std::cout << "  P register: 0x" << std::hex << std::setw(2) << (int)cpu.get_p() << std::dec << "\n";
        std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << "\n\n";
        
    } while (cycle_count < 6 && cpu.get_cycle_step() != 0);
    
    std::cout << "Final result: Memory[$0010] = 0x" << std::hex << std::setw(2) << (int)memory[0x0010] << std::dec << "\n";
    std::cout << "Expected: 0x80\n";
    std::cout << "Success: " << (memory[0x0010] == 0x80 ? "YES" : "NO") << "\n";
    
    return 0;
}
