#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/memory_bus.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== Detailed ROR Flag Analysis ===" << std::endl;
    
    // Create CPU with 6502 config
    using BusConfig = DefaultBusConfig<CpuVariant::NMOS_6502>;
    using CpuType = fam65xx_cpp::Fam65xxCpu<BusConfig>;
    
    // Create memory bus
    MemoryBus<0x10000> memory_bus;
    
    // Create CPU
    CpuType cpu(memory_bus);
    
    // Reset CPU
    cpu.reset();
    
    // Set up test - ROR $10 (0x66 0x10)
    memory_bus.write(0x0000, 0x66); // ROR $10
    memory_bus.write(0x0001, 0x10); // Zero page address
    memory_bus.write(0x0010, 0x85); // Test value: 10000101
    
    // Set initial state
    cpu.get_registers()[CpuReg::PC] = 0x0000;
    cpu.get_registers()[CpuReg::P] = 0x24; // No carry initially
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "P register: 0x" << std::hex << std::setfill('0') << std::setw(2) 
              << (int)cpu.get_registers()[CpuReg::P] << std::endl;
    std::cout << "Memory[0x10]: 0x" << std::hex << std::setfill('0') << std::setw(2) 
              << (int)memory_bus.read(0x10) << std::endl;
    
    // Execute instruction step by step
    for (int cycle = 1; cycle <= 6; cycle++) {
        cpu.execute_cycle();
        
        std::cout << "After cycle " << cycle << ":" << std::endl;
        std::cout << "  P register: 0x" << std::hex << std::setfill('0') << std::setw(2) 
                  << (int)cpu.get_registers()[CpuReg::P];
        
        // Break down P register flags
        uint8_t p = cpu.get_registers()[CpuReg::P];
        std::cout << " (N=" << ((p & 0x80) ? 1 : 0)
                  << " V=" << ((p & 0x40) ? 1 : 0) 
                  << " B=" << ((p & 0x10) ? 1 : 0)
                  << " D=" << ((p & 0x08) ? 1 : 0)
                  << " I=" << ((p & 0x04) ? 1 : 0)
                  << " Z=" << ((p & 0x02) ? 1 : 0)
                  << " C=" << ((p & 0x01) ? 1 : 0) << ")" << std::endl;
        
        std::cout << "  Memory[0x10]: 0x" << std::hex << std::setfill('0') << std::setw(2) 
                  << (int)memory_bus.read(0x10) << std::endl;
        std::cout << "  DL register: 0x" << std::hex << std::setfill('0') << std::setw(2) 
                  << (int)cpu.get_registers()[CpuReg::DL] << std::endl;
        
        // Check if instruction is complete
        if (cpu.get_registers()[CpuReg::PC] == 0x0002) {
            std::cout << "  Instruction completed!" << std::endl;
            break;
        }
    }
    
    std::cout << "\nFinal analysis:" << std::endl;
    uint8_t result = memory_bus.read(0x10);
    uint8_t p_final = cpu.get_registers()[CpuReg::P];
    
    std::cout << "Result: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)result;
    std::cout << " (binary: ";
    for (int i = 7; i >= 0; i--) {
        std::cout << ((result >> i) & 1);
    }
    std::cout << ")" << std::endl;
    
    std::cout << "Expected result: 0x42 (ROR 0x85 with carry=0 should give 0x42)" << std::endl;
    std::cout << "Expected negative flag: 0 (bit 7 of 0x42 is 0)" << std::endl;
    std::cout << "Expected carry flag: 1 (bit 0 of 0x85 is 1)" << std::endl;
    
    std::cout << "\nFlag analysis:" << std::endl;
    std::cout << "Negative flag: " << ((p_final & 0x80) ? 1 : 0) << " (expected: 0)" << std::endl;
    std::cout << "Zero flag: " << ((p_final & 0x02) ? 1 : 0) << " (expected: 0)" << std::endl;
    std::cout << "Carry flag: " << ((p_final & 0x01) ? 1 : 0) << " (expected: 1)" << std::endl;
    
    return 0;
}