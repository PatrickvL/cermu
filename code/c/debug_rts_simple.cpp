#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <iomanip>

// Simple test to debug RTS opcode fetch
int main() {
    std::cout << "=== RTS Simple Debug Test ===" << std::endl;
    
    // Create CPU instance
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    cpu.init_for_test();
    
    // Set up test scenario
    cpu.set_pc(0x4147);  // PC where RTS opcode is located
    cpu.set_p(0xa6);     // Initial P status (V=0 expected)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "P: 0x" << std::hex << (int)cpu.get_p() << std::endl;
    std::cout << "V flag: " << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << std::endl;
    
    // Create proper bus state using system_lines.h macros
    auto create_bus_state = [](uint16_t addr, uint8_t data) -> bus_state_t {
        bus_state_t state = 0;
        BUS_SET_ADDR(state, addr);
        BUS_SET_DATA(state, data);
        state |= BUS_BIT(BUS_RDY_BIT); // RDY high (ready)
        return state;
    };
    
    // Step 1: Opcode fetch (should read 0x60 - RTS)
    std::cout << "=== Cycle 0: Opcode Fetch (RTS = 0x60) ===" << std::endl;
    bus_state_t bus_state = create_bus_state(0x4147, 0x60); // CORRECT RTS opcode
    std::cout << "Bus data being provided: 0x" << std::hex << (int)BUS_GET_DATA(bus_state) << " (RTS)" << std::endl;
    std::cout << "Bus address: 0x" << std::hex << BUS_GET_ADDR(bus_state) << std::endl;
    std::cout << "Before cycle 0: P=0x" << std::hex << (int)cpu.get_p()
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 0: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "Opcode read by CPU: 0x" << std::hex << cpu.get_opcode() << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // If the opcode is wrong, there's a bus state issue
    if (cpu.get_opcode() != 0x60) {
        std::cout << "❌ ERROR: CPU read wrong opcode!" << std::endl;
        std::cout << "Expected: 0x60 (RTS)" << std::endl;
        std::cout << "Actual: 0x" << std::hex << cpu.get_opcode() << std::endl;
        return 1;
    }
    
    // Step 2: RTS Cycle 1 - READ_PC (internal operation)
    std::cout << "=== Cycle 1: RTS Internal ===" << std::endl;
    bus_state = create_bus_state(0x4148, 0x00); // Dummy data
    std::cout << "Before cycle 1: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "Current opcode in CPU: 0x" << std::hex << cpu.get_opcode() << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle 1: P=0x" << std::hex << (int)cpu.get_p() 
              << ", V=" << ((cpu.get_p() & 0x40) ? 1 : 0) << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Check if V flag was corrupted during cycle 1
    if ((cpu.get_p() & 0x40) != 0) {
        std::cout << "❌ ERROR: V flag corrupted during cycle 1!" << std::endl;
        std::cout << "This means SO pin processing is still active for RTS" << std::endl;
        return 1;
    }
    
    std::cout << "✅ V flag preserved correctly in cycle 1!" << std::endl;
    return 0;
}