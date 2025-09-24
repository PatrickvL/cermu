#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

void debug_nop_cycle_table() {
    TestCPU cpu;
    
    std::cout << "=== NOP (0xEA) Cycle Table Debug ===" << std::endl;
    
    // Check 0xEA cycle table
    auto& cycle_table_ea = cpu.template get_cycle_table<0xEA>();
    std::cout << "0xEA cycle count: " << cycle_table_ea.size() << std::endl;
    for (size_t i = 0; i < cycle_table_ea.size(); i++) {
        auto cycle = cycle_table_ea[i];
        std::cout << "  Cycle " << i << ": mem=" << static_cast<int>(cycle.mem_op)
                  << ", data=" << static_cast<int>(cycle.data_op)
                  << ", alu=" << static_cast<int>(cycle.alu_op) << std::endl;
    }
    
    std::cout << "\n=== NOP #$42 (0x80) Cycle Table Debug ===" << std::endl;
    
    // Check 0x80 cycle table
    auto& cycle_table_80 = cpu.template get_cycle_table<0x80>();
    std::cout << "0x80 cycle count: " << cycle_table_80.size() << std::endl;
    for (size_t i = 0; i < cycle_table_80.size(); i++) {
        auto cycle = cycle_table_80[i];
        std::cout << "  Cycle " << i << ": mem=" << static_cast<int>(cycle.mem_op)
                  << ", data=" << static_cast<int>(cycle.data_op)
                  << ", alu=" << static_cast<int>(cycle.alu_op) << std::endl;
    }
}

int main() {
    debug_nop_cycle_table();
    return 0;
}