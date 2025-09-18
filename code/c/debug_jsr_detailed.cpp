#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"

using namespace fam65xx_cpp;

// Create a basic CPU configuration
using cpu_config = cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, 0x10, false, false, false>;
using fam65xx_cpu = fam65xx<cpu_config>;

int main() {
    std::cout << "=== JSR Detailed Debug Test ===" << std::endl;
    
    fam65xx_cpu cpu;
    
    // Setup memory for JSR instruction
    cpu.memory[0x0800] = 0x20; // JSR opcode
    cpu.memory[0x0801] = 0x34; // Low byte of target address
    cpu.memory[0x0802] = 0x12; // High byte of target address
    
    // Initialize CPU state
    cpu.reg[CpuReg::PCL] = 0x00;
    cpu.reg[CpuReg::PCH] = 0x08;
    cpu.reg[CpuReg::S] = 0xff;
    cpu.reg[CpuReg::P] = 0x30;
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
              << ((cpu.reg[CpuReg::PCH] << 8) | cpu.reg[CpuReg::PCL]) << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.reg[CpuReg::S] << std::endl;
    std::cout << "  P: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.reg[CpuReg::P] << std::endl;
    std::cout << std::endl;
    
    // Execute cycles step by step
    for (int cycle = 0; cycle < 7; cycle++) {
        std::cout << "Cycle " << cycle << ": ";
        
        if (cycle == 0) {
            std::cout << "Opcode fetch" << std::endl;
            cpu.clock();
        } else {
            cpu.clock();
            if (cycle == 3 || cycle == 4) {
                // For JSR stack push cycles, check what values are being calculated
                std::cout << "JSR Stack Push - ";
                uint16_t current_pc = (cpu.reg[CpuReg::PCH] << 8) | cpu.reg[CpuReg::PCL];
                std::cout << "PC=0x" << std::hex << std::setw(4) << std::setfill('0') << current_pc;
                
                // Check what the JSR logic should calculate
                if (cycle == 3) {
                    uint8_t expected_push = (current_pc >> 8) & 0xFF;
                    std::cout << " -> Should push PCH=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)expected_push;
                } else if (cycle == 4) {
                    uint8_t expected_push = current_pc & 0xFF;
                    std::cout << " -> Should push PCL=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)expected_push;
                }
                std::cout << std::endl;
            } else {
                std::cout << std::endl;
            }
        }
        
        std::cout << "  After: PC=0x" << std::hex << std::setw(4) << std::setfill('0') 
                  << ((cpu.reg[CpuReg::PCH] << 8) | cpu.reg[CpuReg::PCL])
                  << " SP=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.reg[CpuReg::S]
                  << " Cycle=" << std::dec << cpu.cycle_step << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Final state:" << std::endl;
    uint16_t final_pc = (cpu.reg[CpuReg::PCH] << 8) | cpu.reg[CpuReg::PCL];
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << final_pc
              << " (should be 0x1234)" << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.reg[CpuReg::S] << " (should be 0xfd - decremented by 2)" << std::endl;
    std::cout << "  P: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.reg[CpuReg::P] << " (should be preserved)" << std::endl;
    std::cout << std::endl;
    
    // Check stack contents
    std::cout << "Stack contents:" << std::endl;
    std::cout << "  Stack[0x01ff]: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.memory[0x01ff] << " (should be 0x08)" << std::endl;
    std::cout << "  Stack[0x01fe]: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.memory[0x01fe] << " (should be 0x02)" << std::endl;
    std::cout << std::endl;
    
    bool success = (final_pc == 0x1234) && 
                   (cpu.reg[CpuReg::S] == 0xfd) &&
                   (cpu.memory[0x01ff] == 0x08) &&
                   (cpu.memory[0x01fe] == 0x02);
    
    std::cout << "JSR Test: " << (success ? "PASS" : "FAIL") << std::endl;
    
    if (!success) {
        std::cout << std::endl << "Expected:" << std::endl;
        std::cout << "  PC: 0x1234, SP: 0xfd" << std::endl;
        std::cout << "  Stack[0x01ff]: 0x08 (return address high)" << std::endl;
        std::cout << "  Stack[0x01fe]: 0x02 (return address low)" << std::endl;
    }
    
    return success ? 0 : 1;
}