#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>

using namespace fam65xx_cpp;
using TestConfig = config_6502;

void print_registers(const fam65xx<TestConfig>& cpu, const std::string& label) {
    printf("%s: A=%02X X=%02X Y=%02X P=%02X S=%02X PC=%04X DL=%02X\n", 
           label.c_str(),
           cpu.get_a(), cpu.get_x(), cpu.get_y(), cpu.get_p(), cpu.get_s(), cpu.get_pc(),
           cpu.get_reg(CpuReg::DL));
}

int main() {
    printf("=== Debug NOP Register Changes ===\n");
    
    // Test basic NOP (0xEA)
    {
        fam65xx<TestConfig> cpu;
        cpu.init_for_test();
        cpu.set_pc(0x1000);
        cpu.set_a(0x42);
        cpu.set_x(0x33);
        cpu.set_y(0x44);
        cpu.set_p(0x55);
        cpu.set_s(0xFD);
        
        print_registers(cpu, "Before NOP");
        
        // Set up memory
        uint8_t memory[65536] = {0};
        memory[0x1000] = 0xEA; // NOP opcode
        
        // Execute instruction cycle by cycle until complete
        bus_state_t bus_state = 0;
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        int cycles = 0;
        
        do {
            uint16_t addr = cpu.get_address();
            bool is_read = cpu.get_rw();
            
            if (is_read) {
                uint8_t data = memory[addr];
                bus_state = BUS_SET_DATA(bus_state, data);
            } else {
                uint8_t data = cpu.get_write_data();
                memory[addr] = data;
                bus_state = BUS_SET_DATA(bus_state, data);
            }
            
            bus_state = cpu.cycle_tick(bus_state);
            cycles++;
            printf("  Cycle %d: step=%d PC=%04X\n", cycles, cpu.get_cycle_step(), cpu.get_pc());
            
        } while (cpu.get_cycle_step() != 0 && cycles < 10);
        
        print_registers(cpu, "After NOP complete");
        
        printf("\n");
    }
    
    // Test immediate mode NOP (0x80)
    {
        fam65xx<TestConfig> cpu;
        cpu.init_for_test();
        cpu.set_pc(0x1000);
        cpu.set_a(0x42);
        cpu.set_x(0x33);
        cpu.set_y(0x44);
        cpu.set_p(0x55);
        cpu.set_s(0xFD);
        
        print_registers(cpu, "Before NOP #$42");
        
        // Set up memory
        uint8_t memory[65536] = {0};
        memory[0x1000] = 0x80; // NOP #$nn opcode
        memory[0x1001] = 0x42; // Immediate operand
        
        // Execute instruction cycle by cycle until complete
        bus_state_t bus_state = 0;
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        int cycles = 0;
        
        do {
            uint16_t addr = cpu.get_address();
            bool is_read = cpu.get_rw();
            
            if (is_read) {
                uint8_t data = memory[addr];
                bus_state = BUS_SET_DATA(bus_state, data);
            } else {
                uint8_t data = cpu.get_write_data();
                memory[addr] = data;
                bus_state = BUS_SET_DATA(bus_state, data);
            }
            
            bus_state = cpu.cycle_tick(bus_state);
            cycles++;
            printf("  Cycle %d: step=%d PC=%04X\n", cycles, cpu.get_cycle_step(), cpu.get_pc());
            
        } while (cpu.get_cycle_step() != 0 && cycles < 10);
        
        print_registers(cpu, "After NOP #$42 complete");
        
        printf("\n");
    }
    
    return 0;
}