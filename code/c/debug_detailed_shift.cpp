
#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

int main() {
    using namespace fam65xx_cpp;
    
    fam65xx_with_cycle_count<config_6502> cpu;
    cpu.init_for_test();
    
    uint8_t memory[0x10000];
    std::fill(memory, memory + sizeof(memory), 0xEA);
    
    // ASL $10 test
    memory[0x0000] = 0x06;  // ASL $10 opcode
    memory[0x0001] = 0x10;  // Zero page address  
    memory[0x0010] = 0x40;  // Memory value to shift
    
    cpu.set_pc(0x0000);
    
    std::cout << "=== DETAILED DEBUG: ASL $10 Memory-Mode Operation ===" << std::endl;
    std::cout << "Initial: Memory[$10]=0x" << std::hex << (int)memory[0x10] << std::endl;
    
    bus_state_t bus_state = 0;
    int cycle_count = 0;
    
    do {
        cycle_count++;
        uint16_t addr = cpu.get_address();
        uint8_t data = memory[addr];
        bool is_write = !cpu.get_rw();
        
        std::cout << "=== Cycle " << cycle_count << " ===" << std::endl;
        std::cout << "  Step: " << (int)cpu.get_cycle_step() << std::endl;
        std::cout << "  Opcode: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_opcode() << std::endl;
        std::cout << "  DL register: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_reg(CpuReg::DL) << std::endl;
        
        if (is_write) {
            uint8_t write_data = cpu.get_write_data();
            std::cout << "  WRITE: address=0x" << std::hex << std::setfill('0') << std::setw(4) << addr;
            std::cout << " data=0x" << std::setw(2) << (int)write_data << std::endl;
            memory[addr] = write_data;
        } else {
            std::cout << "  READ:  address=0x" << std::hex << std::setfill('0') << std::setw(4) << addr;
            std::cout << " data=0x" << std::setw(2) << (int)data << std::endl;
        }
        
        bus_state = BUS_SET_DATA(bus_state, data);
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "  After cycle_tick - DL register: 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_reg(CpuReg::DL) << std::endl;
        std::cout << std::endl;
        
    } while (cycle_count < 6 && cpu.get_cycle_step() != 0);
    
    std::cout << "Final result: Memory[$10]=0x" << std::hex << (int)memory[0x10] << std::endl;
    std::cout << "Expected: 0x80" << std::endl;
    
    return 0;
}
