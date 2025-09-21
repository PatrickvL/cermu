#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

int main() {
    std::cout << "=== Testing Memory Shift/Rotate Operations ===" << std::endl;
    
    TestCPU cpu;
    cpu.init_for_test();
    
    // Test ASL $10 (0x06) - Arithmetic Shift Left zero page
    cpu.set_pc(0x1000);
    
    // Create memory with the instruction
    std::array<uint8_t, 65536> memory = {};
    memory[0x1000] = 0x06;  // ASL $nn opcode
    memory[0x1001] = 0x10;  // Zero page address
    memory[0x0010] = 0x42;  // Value to shift (0x42 -> 0x84)
    
    std::cout << "Testing ASL $10 (shift 0x42 left)" << std::endl;
    std::cout << "Initial: Memory[0x10] = 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)memory[0x0010] << std::endl;
    
    // Execute instruction cycle by cycle
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);  // Set RDY high (ready)
    int cycles = 0;
    
    std::cout << "\nExecution trace:" << std::endl;
    do {
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        std::cout << "Cycle " << std::dec << cycles + 1 << ": ";
        std::cout << "Addr=0x" << std::hex << std::setw(4) << addr;
        std::cout << " " << (is_read ? "READ" : "WRITE");
        
        if (is_read) {
            // Read operation
            uint8_t data = memory[addr];
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << " data=0x" << std::setw(2) << (int)data;
        } else {
            // Write operation  
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << " data=0x" << std::setw(2) << (int)data;
        }
        
        std::cout << " (step=" << std::dec << (int)cpu.get_cycle_step() << ")";
        std::cout << std::endl;
        
        bus_state = cpu.cycle_tick(bus_state);
        cycles++;
    } while (cpu.get_cycle_step() != 0 && cycles < 15);
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "Memory[0x10] = 0x" << std::hex << std::setw(2) << (int)memory[0x0010] << std::endl;
    std::cout << "CPU P = 0x" << std::setw(2) << (int)cpu.get_p() << std::endl;
    std::cout << "PC = 0x" << std::setw(4) << cpu.get_pc() << std::endl;
    std::cout << "Cycles executed: " << std::dec << cycles << std::endl;
    
    // Expected results for ASL $10 with initial value 0x42:
    // - Memory[0x10] should be 0x84 (0x42 << 1)
    // - Carry flag should be 0 (bit 7 of 0x42 was 0)
    // - Negative flag should be 1 (result 0x84 has bit 7 set)
    // - Zero flag should be 0 (result is not zero)
    // - PC should be 0x1002 (instruction is 2 bytes)
    
    bool memory_correct = (memory[0x0010] == 0x84);
    bool pc_correct = (cpu.get_pc() == 0x1002);
    bool carry_correct = !(cpu.get_p() & 0x01); // Carry should be clear
    bool negative_correct = (cpu.get_p() & 0x80); // Negative should be set
    
    std::cout << "\nValidation:" << std::endl;
    std::cout << "Memory result: " << (memory_correct ? "PASS" : "FAIL") << " (expected 0x84, got 0x" << std::hex << std::setw(2) << (int)memory[0x0010] << ")" << std::endl;
    std::cout << "PC result: " << (pc_correct ? "PASS" : "FAIL") << " (expected 0x1002, got 0x" << std::setw(4) << cpu.get_pc() << ")" << std::endl;
    std::cout << "Carry flag: " << (carry_correct ? "PASS" : "FAIL") << " (should be clear)" << std::endl;
    std::cout << "Negative flag: " << (negative_correct ? "PASS" : "FAIL") << " (should be set)" << std::endl;
    
    bool all_pass = memory_correct && pc_correct && carry_correct && negative_correct;
    std::cout << "\nOverall: " << (all_pass ? "✅ PASS" : "❌ FAIL") << std::endl;
    
    return all_pass ? 0 : 1;
}