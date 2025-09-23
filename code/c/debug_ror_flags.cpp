#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== Debugging ROR Flag Calculation ===\n";
    
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    cpu.init_for_test();
    
    // Test case that's failing: ROR $nn with value 0x01, carry=0
    // Expected result: memory becomes 0x00, N=0, Z=1, C=1
    
    // Set up memory and CPU state
    uint8_t memory[0x10000] = {0};
    memory[0x1000] = 0x66; // ROR $nn opcode
    memory[0x1001] = 0x10; // Zero page address $10
    memory[0x0010] = 0x01; // Value to rotate: 0x01
    
    // Set PC to start of instruction
    cpu.set_pc(0x1000);
    cpu.set_p(0x30); // Clear carry flag, set other typical flags
    
    std::cout << "Initial state:\n";
    std::cout << "  Memory[$10] = 0x" << std::hex << (int)memory[0x0010] << "\n";
    std::cout << "  P register = 0x" << std::hex << (int)cpu.get_p() << "\n";
    std::cout << "  Carry flag = " << ((cpu.get_p() & 0x01) ? 1 : 0) << "\n";
    
    // Execute instruction cycle by cycle
    auto bus_state = uint32_t(0x00000000) | (1 << 16); // RDY high
    
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        if (is_write) {
            uint8_t write_data = cpu.get_write_data();
            memory[addr] = write_data;
            std::cout << "Cycle " << cycle << ": WRITE to $" << std::hex << addr 
                      << " = 0x" << std::hex << (int)write_data << "\n";
        } else {
            uint8_t read_data = memory[addr];
            bus_state = (bus_state & 0xFFFFFF00) | read_data;
            std::cout << "Cycle " << cycle << ": READ from $" << std::hex << addr 
                      << " = 0x" << std::hex << (int)read_data << "\n";
        }
        
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "  PC = 0x" << std::hex << cpu.get_pc() 
                  << ", P = 0x" << std::hex << (int)cpu.get_p()
                  << ", Step = " << (int)cpu.get_cycle_step() << "\n";
        
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            std::cout << "Instruction completed after " << cycle + 1 << " cycles\n";
            break;
        }
    }
    
    std::cout << "\nFinal state:\n";
    std::cout << "  Memory[$10] = 0x" << std::hex << (int)memory[0x0010] << "\n";
    std::cout << "  P register = 0x" << std::hex << (int)cpu.get_p() << "\n";
    std::cout << "  N flag = " << ((cpu.get_p() & 0x80) ? 1 : 0) << " (expected: 0)\n";
    std::cout << "  Z flag = " << ((cpu.get_p() & 0x02) ? 1 : 0) << " (expected: 1)\n";
    std::cout << "  C flag = " << ((cpu.get_p() & 0x01) ? 1 : 0) << " (expected: 1)\n";
    
    // Check expected vs actual
    bool n_correct = ((cpu.get_p() & 0x80) == 0);
    bool z_correct = ((cpu.get_p() & 0x02) != 0);
    bool c_correct = ((cpu.get_p() & 0x01) != 0);
    bool result_correct = (memory[0x0010] == 0x00);
    
    std::cout << "\nResults:\n";
    std::cout << "  Memory result: " << (result_correct ? "✅ PASS" : "❌ FAIL") << "\n";
    std::cout << "  N flag: " << (n_correct ? "✅ PASS" : "❌ FAIL") << "\n";
    std::cout << "  Z flag: " << (z_correct ? "✅ PASS" : "❌ FAIL") << "\n";
    std::cout << "  C flag: " << (c_correct ? "✅ PASS" : "❌ FAIL") << "\n";
    
    return (n_correct && z_correct && c_correct && result_correct) ? 0 : 1;
}