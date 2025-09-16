#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

int main() {
    std::cout << "=== ASL Accumulator Debug Test ===" << std::endl;
    
    // Create CPU with 6502 configuration
    using Config = fam65xx_cpp::cpu_config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    
    // Initialize CPU for testing
    cpu.init_for_test();
    
    // Set up a simple test case: ASL on accumulator value 0x39
    cpu.set_pc(0x1000);  // Set PC to test address
    cpu.set_a(0x39);     // Set A register to 0x39 (from failing test)
    
    std::cout << "Before ASL:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  A:  0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << std::endl;
    std::cout << "  P:  0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    
    // Get cycle description for ASL accumulator (opcode 0x0A, cycle 1)
    auto cycle = cpu.GET_CYCLE(0x0A, 1);
    std::cout << "Cycle description:" << std::endl;
    std::cout << "  MemOp: " << (int)cycle.get_mem_op() << std::endl;
    std::cout << "  DataOp: " << (int)cycle.get_data_op() << std::endl;
    std::cout << "  AluOp: " << (int)cycle.get_alu_op() << std::endl;
    std::cout << "  IsSync: " << cycle.is_sync() << std::endl;
    
    // Create bus state with ASL opcode
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0x0A);  // ASL accumulator opcode
    bus_state = BUS_SET_ADDRESS(bus_state, 0x1000);
    
    // Execute opcode fetch (cycle 0)
    std::cout << "\nExecuting opcode fetch (cycle 0):" << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After opcode fetch:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << "  Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << cpu.get_opcode() << std::endl;
    
    // Execute ASL instruction (cycle 1)
    std::cout << "\nExecuting ASL instruction (cycle 1):" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, 0x00);  // Dummy data since we don't read memory
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After ASL execution:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  A:  0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << std::endl;
    std::cout << "  P:  0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    std::cout << "  Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Expected results:
    // A should be 0x72 (0x39 << 1)
    // C flag should be 0 (bit 7 of 0x39 was 0)
    // N flag should be 0 (bit 7 of 0x72 is 0)
    // Z flag should be 0 (0x72 != 0)
    // PC should be 0x1001 (advanced by 1)
    
    std::cout << "\nExpected results:" << std::endl;
    std::cout << "  PC: 0x1001" << std::endl;
    std::cout << "  A:  0x72" << std::endl;
    std::cout << "  C:  0 (no carry out)" << std::endl;
    std::cout << "  N:  0 (result positive)" << std::endl;
    std::cout << "  Z:  0 (result non-zero)" << std::endl;
    
    return 0;
}