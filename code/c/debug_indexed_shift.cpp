#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

int main() {
    std::cout << "=== Debug Indexed Shift Operation ASL $nn,X ===" << std::endl;
    
    TestCPU cpu;
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    cpu.set_x(0x05);  // X offset
    
    // Test ASL $10,X (0x16) - should target address $15
    std::array<uint8_t, 65536> memory = {};
    memory[0x1000] = 0x16;  // ASL $nn,X opcode
    memory[0x1001] = 0x10;  // Base address
    memory[0x0015] = 0x42;  // Value at $10 + X($05) = $15
    
    std::cout << "Testing ASL $10,X with X=0x05" << std::endl;
    std::cout << "Target address: 0x" << std::hex << (0x10 + 0x05) << std::endl;
    std::cout << "Initial Memory[0x15] = 0x" << (int)memory[0x0015] << std::endl;
    
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    
    for (int cycle = 1; cycle <= 10; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        uint8_t step_before = cpu.get_cycle_step();
        
        std::cout << "\n--- Cycle " << std::dec << cycle << " ---" << std::endl;
        std::cout << "  Address: 0x" << std::hex << std::setw(4) << std::setfill('0') << addr;
        std::cout << ", R/W: " << (is_read ? "READ" : "WRITE");
        std::cout << ", Step: " << std::dec << (int)step_before << std::endl;
        
        // Check cycle table for this step
        if (step_before > 0) {
            auto cycle_desc = TestCPU::GET_CYCLE(0x16, step_before);
            std::cout << "  Cycle table: MemOp=" << (int)cycle_desc.mem_op 
                      << " DataOp=" << (int)cycle_desc.data_op 
                      << " AluOp=" << (int)cycle_desc.alu_op 
                      << " Sync=" << (cycle_desc.is_sync() ? "true" : "false") << std::endl;
        }
        
        // Handle memory access
        if (is_read) {
            uint8_t data = memory[addr];
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << "  Reading data: 0x" << std::hex << std::setw(2) << (int)data << std::endl;
        } else {
            uint8_t data = cpu.get_write_data();
            memory[addr] = data;
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << "  Writing data: 0x" << std::hex << std::setw(2) << (int)data << std::endl;
        }
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        uint8_t step_after = cpu.get_cycle_step();
        std::cout << "  After: Step=" << std::dec << (int)step_after;
        std::cout << ", Memory[0x15]=0x" << std::hex << std::setw(2) << (int)memory[0x0015];
        std::cout << ", Flags=0x" << std::setw(2) << (int)cpu.get_p() << std::endl;
        
        // Check if instruction completed
        if (step_after == 0) {
            std::cout << "\n✅ INSTRUCTION COMPLETED after " << cycle << " cycles" << std::endl;
            break;
        }
        
        if (cycle >= 10) {
            std::cout << "\n❌ TIMEOUT after 10 cycles" << std::endl;
            break;
        }
    }
    
    std::cout << "\nFinal results:" << std::endl;
    std::cout << "Memory[0x15] = 0x" << std::hex << std::setw(2) << (int)memory[0x0015] << " (expected 0x84)" << std::endl;
    std::cout << "CPU P = 0x" << std::setw(2) << (int)cpu.get_p() << std::endl;
    std::cout << "PC = 0x" << std::setw(4) << cpu.get_pc() << " (expected 0x1002)" << std::endl;
    
    return 0;
}