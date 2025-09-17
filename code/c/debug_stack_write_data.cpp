#include <iostream>
#include <iomanip>
#include "tests/fam65xx_cpp_test_harness.h"

int main() {
    std::cout << "=== Debug Stack Write Data ===\n";
    
    // Create CPU instance
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.power_on();
    cpu.reset();
    
    // Set up test state
    cpu.reg.A = 0x42;
    cpu.reg.P = 0xc3;
    cpu.reg.S = 0xff;
    cpu.reg.PC = 0x1000;
    
    // Write PHA instruction at 0x1000
    cpu.bus_write_8(0x1000, 0x48);  // PHA opcode
    cpu.bus_write_8(0x1001, 0xea);  // NOP (dummy)
    
    std::cout << "Initial state:\n";
    std::cout << "  A=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.reg.A;
    std::cout << " P=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.reg.P;
    std::cout << " S=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.reg.S;
    std::cout << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.reg.PC << "\n";
    
    // Execute one step at a time
    for (int step = 0; step < 10; step++) {
        std::cout << "\n--- Step " << step + 1 << " ---\n";
        
        // Get current cycle info
        auto cycle = cpu.get_current_cycle();
        std::cout << "Cycle type: " << (int)cycle.type << "\n";
        std::cout << "Memory op: " << (int)cycle.mem_op << "\n";
        std::cout << "Data op: " << (int)cycle.data_op << "\n";
        std::cout << "Address: 0x" << std::hex << std::setw(4) << std::setfill('0') << cycle.address << "\n";
        
        // Check pending_data before step
        std::cout << "pending_data before: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.pending_data << "\n";
        
        // Execute one cycle
        cpu.step();
        
        // Check state after step
        std::cout << "pending_data after: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.pending_data << "\n";
        std::cout << "Stack[0x01FF]: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.bus_read_8(0x01FF) << "\n";
        std::cout << "A=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.reg.A;
        std::cout << " S=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.reg.S;
        std::cout << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.reg.PC << "\n";
        
        // Stop if instruction complete
        if (cpu.reg.PC != 0x1000 && step > 2) {
            std::cout << "\nInstruction complete!\n";
            break;
        }
    }
    
    std::cout << "\n=== Debug Complete ===\n";
    return 0;
}