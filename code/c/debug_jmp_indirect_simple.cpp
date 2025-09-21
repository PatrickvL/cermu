#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

template<typename CpuType>
void print_state(const CpuType& cpu, int cycle) {
    std::cout << "Cycle " << cycle << ": ";
    std::cout << "PC=" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc();
    std::cout << " A=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a();
    std::cout << " X=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_x();
    std::cout << " Y=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_y();
    std::cout << " P=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p();
    std::cout << " S=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_s();
    std::cout << " ABL=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_reg(CpuReg::ABL);
    std::cout << " ABH=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_reg(CpuReg::ABH);
    std::cout << " DL=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_reg(CpuReg::DL);
    std::cout << " Opcode=" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_opcode();
    std::cout << " Step=" << (int)cpu.get_cycle_step();
    std::cout << std::endl;
}

int main() {
    // Create 6502 CPU
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up a simple JMP indirect test
    // Memory layout:
    // $1000: 6C 34 12    ; JMP ($1234)
    // $1234: 78 56       ; Target address $5678
    
    uint8_t memory[0x10000] = {0};
    
    // Set up instruction at $1000
    memory[0x1000] = 0x6C;  // JMP indirect
    memory[0x1001] = 0x34;  // Low byte of indirect address
    memory[0x1002] = 0x12;  // High byte of indirect address
    
    // Set up target address at $1234
    memory[0x1234] = 0x78;  // Target low byte
    memory[0x1235] = 0x56;  // Target high byte
    
    // Set PC to start of instruction
    cpu.set_pc(0x1000);
    
    std::cout << "=== JMP Indirect Test ===" << std::endl;
    std::cout << "Instruction: JMP ($1234)" << std::endl;
    std::cout << "Target address at $1234: $5678" << std::endl;
    std::cout << "Expected final PC: $5678" << std::endl;
    std::cout << std::endl;
    
    print_state(cpu, 0);
    
    // Execute instruction cycle by cycle
    for (int cycle = 1; cycle <= 10; cycle++) {
        // Set up bus state with memory data
        bus_state_t bus_state = 0;
        
        // Get the address the CPU wants to read from
        uint16_t addr = cpu.get_address();
        uint8_t data = memory[addr];
        
        std::cout << "Cycle " << cycle << " - Reading from $" << std::hex << std::setw(4) << std::setfill('0') << addr;
        std::cout << " -> $" << std::hex << std::setw(2) << std::setfill('0') << (int)data << std::endl;
        
        // Set the data on the bus
        BUS_SET_DATA(bus_state, data);
        BUS_SET_ADDR(bus_state, addr);
        
        // Execute the cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        print_state(cpu, cycle);
        
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0) {
            std::cout << "Instruction complete!" << std::endl;
            break;
        }
        
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Final PC: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "Expected: $5678" << std::endl;
    
    if (cpu.get_pc() == 0x5678) {
        std::cout << "SUCCESS!" << std::endl;
        return 0;
    } else {
        std::cout << "FAILED!" << std::endl;
        return 1;
    }
}