#include <iostream>
#include <iomanip>
#include <array>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

int main() {
    std::cout << "=== INC DL Register Debug Test ===" << std::endl;
    
    TestCPU cpu;
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    cpu.set_p(0x24); // Clear all flags except unused
    
    // Set up INC $10 (0xE6 0x10)
    std::array<uint8_t, 65536> memory = {};
    memory[0x1000] = 0xE6; // INC $nn
    memory[0x1001] = 0x10; // Zero page address
    memory[0x0010] = 0x42; // Test value: 0x42
    
    std::cout << "Initial value at $10: 0x" << std::hex << (int)memory[0x0010] << std::endl;
    std::cout << "Initial DL register: 0x" << std::hex << (int)cpu.get_reg(CpuReg::DL) << std::endl;
    
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    int cycles = 0;
    
    do {
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        std::cout << "Cycle " << (cycles + 1) << ": addr=0x" << std::hex << addr
                  << ", R/W=" << (is_read ? "R" : "W");
        
        if (is_read) {
            uint8_t data = memory[addr];
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << ", read data=0x" << std::hex << (int)data;
        } else {
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << ", write data=0x" << std::hex << (int)data;
        }
        
        bus_state = cpu.cycle_tick(bus_state);
        cycles++;
        
        std::cout << ", DL=0x" << std::hex << (int)cpu.get_reg(CpuReg::DL)
                  << ", P=0x" << std::hex << (int)cpu.get_p()
                  << ", mem[0x10]=0x" << std::hex << (int)memory[0x0010] << std::endl;
        
    } while (cpu.get_cycle_step() != 0 && cycles < 10);
    
    std::cout << "\nFinal result:" << std::endl;
    std::cout << "Value at $10: 0x" << std::hex << (int)memory[0x0010] << " (expected: 0x43)" << std::endl;
    std::cout << "Final DL register: 0x" << std::hex << (int)cpu.get_reg(CpuReg::DL) << std::endl;
    
    bool result_correct = (memory[0x0010] == 0x43);
    std::cout << "Result: " << (result_correct ? "✅ PASS" : "❌ FAIL") << std::endl;
    
    return result_correct ? 0 : 1;
}