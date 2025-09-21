#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

int main() {
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    
    std::cout << "=== Address Calculation Debug ===" << std::endl;
    
    // Initialize for testing
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    
    std::cout << "PC set to: 0x" << std::hex << cpu.get_pc() << std::endl;
    
    // Check what address get_address() returns
    uint16_t address = cpu.get_address();
    std::cout << "get_address() returns: 0x" << std::hex << address << std::endl;
    
    // Check cycle step
    std::cout << "cycle_step: " << std::dec << (int)cpu.get_cycle_step() << std::endl;
    std::cout << "opcode: 0x" << std::hex << cpu.get_opcode() << std::endl;
    
    // Check if this is step 0 (opcode fetch)
    if (cpu.get_cycle_step() == 0) {
        std::cout << "This is step 0 - should fetch opcode from PC" << std::endl;
        
        // According to get_address() logic for step 0:
        // "if (cycle_step == 0) return (reg[CpuReg::PCH] << 8) | reg[CpuReg::PCL];"
        uint8_t pcl = cpu.get_reg(static_cast<uint8_t>(CpuReg::PCL));
        uint8_t pch = cpu.get_reg(static_cast<uint8_t>(CpuReg::PCH));
        
        std::cout << "PCL register: 0x" << std::hex << (int)pcl << std::endl;
        std::cout << "PCH register: 0x" << std::hex << (int)pch << std::endl;
        std::cout << "Calculated PC: 0x" << std::hex << ((pch << 8) | pcl) << std::endl;
    }
    
    return 0;
}