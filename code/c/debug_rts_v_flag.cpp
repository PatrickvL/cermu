#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>

// Simple test to debug V flag corruption in RTS
int main() {
    std::cout << "=== RTS V Flag Debug Test ===" << std::endl;
    
    // Create CPU instance
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    cpu.init_for_test();
    
    // Set up test scenario similar to ProcessorTests
    // Set initial state that matches ProcessorTests failing case
    cpu.set_pc(0x4147);  // PC where RTS opcode is located
    cpu.set_a(0x14);     // Test values from ProcessorTests
    cpu.set_x(0x00);
    cpu.set_y(0xe2);
    cpu.set_s(0xfc);     // Stack pointer
    cpu.set_p(0xa6);     // Initial P status (V=0 expected)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "A: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "V flag: " << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << std::endl;
    
    // Create memory with RTS opcode at PC and return address on stack
    auto create_bus_state = [](uint16_t addr, uint8_t data) -> uint32_t {
        return ((uint32_t)addr << 16) | ((uint32_t)data << 8) | 0x01; // RDY high
    };
    
    // Step 1: Opcode fetch (should read 0x60 - RTS)
    std::cout << "=== Cycle 0: Opcode Fetch ===" << std::endl;
    uint32_t bus_state = create_bus_state(0x4147, 0x60);
    std::cout << "Before cycle 0: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 0: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "Opcode: 0x" << std::hex << cpu.get_opcode() << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Step 2: RTS Cycle 1 - READ_PC (internal operation)
    std::cout << "=== Cycle 1: RTS Internal ===" << std::endl;
    bus_state = create_bus_state(0x4148, 0x00); // Dummy data
    std::cout << "Before cycle 1: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 1: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Step 3: RTS Cycle 2 - READ_SP (internal operation)
    std::cout << "=== Cycle 2: RTS Stack Internal ===" << std::endl;
    bus_state = create_bus_state(0x01fc, 0x00); // Stack read
    std::cout << "Before cycle 2: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 2: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Step 4: RTS Cycle 3 - Pull PCL from stack
    std::cout << "=== Cycle 3: Pull PCL ===" << std::endl;
    bus_state = create_bus_state(0x01fd, 0xdd); // PCL = 0xdd
    std::cout << "Before cycle 3: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 3: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "PCL: 0x" << std::hex << (int)cpu.get_reg(CpuReg::PCL) << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Step 5: RTS Cycle 4 - Pull PCH from stack
    std::cout << "=== Cycle 4: Pull PCH ===" << std::endl;
    bus_state = create_bus_state(0x01fe, 0x1d); // PCH = 0x1d
    std::cout << "Before cycle 4: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 4: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "PCH: 0x" << std::hex << (int)cpu.get_reg(CpuReg::PCH) << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Step 6: RTS Cycle 5 - Internal PC increment
    std::cout << "=== Cycle 5: PC Increment ===" << std::endl;
    bus_state = create_bus_state(0x1ddd, 0x00); // Read at incremented PC
    std::cout << "Before cycle 5: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 5: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Step 7: RTS Cycle 6 - Final sync cycle
    std::cout << "=== Cycle 6: Final Sync ===" << std::endl;
    bus_state = create_bus_state(0x1ddd, 0x00); // No memory operation
    std::cout << "Before cycle 6: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 6: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Final result
    std::cout << "=== Final Result ===" << std::endl;
    std::cout << "Expected PC: 0x1ddd" << std::endl;
    std::cout << "Actual PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Expected P: 0xa6 (V=0)" << std::endl;
    std::cout << "Actual P: 0x" << std::hex << (int)cpu.get_p() << " (V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << ")" << std::endl;
    
    if (cpu.get_pc() == 0x1ddd && cpu.get_p() == 0xa6) {
        std::cout << "✅ RTS V flag test PASSED!" << std::endl;
    } else {
        std::cout << "❌ RTS V flag test FAILED!" << std::endl;
        std::cout << "PC difference: " << (int)(cpu.get_pc() - 0x1ddd) << std::endl;
        std::cout << "P difference: 0x" << std::hex << ((int)cpu.get_p() ^ 0xa6) << std::endl;
    }
    
    return 0;
}