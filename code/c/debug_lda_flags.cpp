#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>

int main() {
    using Config = config_6502;
    using CPU = fam65xx_cpp::fam65xx<Config>;
    
    CPU cpu;
    cpu.init_for_test();
    
    std::cout << "=== LDA Flag Handling Analysis ===" << std::endl;
    
    // Test LDA with different values to check N and Z flag setting
    
    // Test 1: LDA #$00 (should set Z flag)
    std::cout << "\n--- Test 1: LDA #$00 (should set Z flag) ---" << std::endl;
    cpu.set_pc(0x0000);
    cpu.set_a(0x00);
    cpu.set_status(0x00); // Clear all flags
    
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0xa9); // LDA immediate
    bus_state = cpu.cycle_tick(bus_state);
    
    bus_state = BUS_SET_DATA(bus_state, 0x00); // Load zero
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "A register: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "Status flags: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "Z flag set: " << ((cpu.get_status() & 0x02) ? "YES" : "NO") << std::endl;
    std::cout << "N flag set: " << ((cpu.get_status() & 0x80) ? "YES" : "NO") << std::endl;
    
    // Test 2: LDA #$80 (should set N flag)
    std::cout << "\n--- Test 2: LDA #$80 (should set N flag) ---" << std::endl;
    cpu.set_pc(0x0000);
    cpu.set_a(0x00);
    cpu.set_status(0x00); // Clear all flags
    
    bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0xa9); // LDA immediate
    bus_state = cpu.cycle_tick(bus_state);
    
    bus_state = BUS_SET_DATA(bus_state, 0x80); // Load negative value
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "A register: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "Status flags: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "Z flag set: " << ((cpu.get_status() & 0x02) ? "YES" : "NO") << std::endl;
    std::cout << "N flag set: " << ((cpu.get_status() & 0x80) ? "YES" : "NO") << std::endl;
    
    // Test 3: LDA #$01 (should clear both N and Z flags)
    std::cout << "\n--- Test 3: LDA #$01 (should clear N and Z flags) ---" << std::endl;
    cpu.set_pc(0x0000);
    cpu.set_a(0x00);
    cpu.set_status(0x82); // Set N and Z flags initially
    
    bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0xa9); // LDA immediate
    bus_state = cpu.cycle_tick(bus_state);
    
    bus_state = BUS_SET_DATA(bus_state, 0x01); // Load positive non-zero value
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "A register: 0x" << std::hex << (int)cpu.get_a() << std::endl;
    std::cout << "Status flags: 0x" << std::hex << (int)cpu.get_status() << std::endl;
    std::cout << "Z flag set: " << ((cpu.get_status() & 0x02) ? "YES" : "NO") << std::endl;
    std::cout << "N flag set: " << ((cpu.get_status() & 0x80) ? "YES" : "NO") << std::endl;
    
    return 0;
}