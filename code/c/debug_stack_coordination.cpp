#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace std;

int main() {
    cout << "=== DEBUGGING STACK COORDINATION ===\n";
    
    // Create CPU
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_a(0x42);
    cpu.set_p(0xc3);
    cpu.set_s(0xff);
    cpu.set_pc(0x1000);
    
    cout << "Initial state:\n";
    cout << "A: $" << hex << setw(2) << setfill('0') << (int)cpu.get_a() << "\n";
    cout << "S: $" << hex << setw(2) << setfill('0') << (int)cpu.get_s() << "\n";
    cout << "PC: $" << hex << setw(4) << setfill('0') << (int)cpu.get_pc() << "\n\n";
    
    // Simulate bus data for PHA instruction
    bus_state_t bus = 0;
    
    // Cycle 1: Opcode fetch (PHA = 0x48)
    cout << "--- Cycle 1 ---\n";
    BUS_SET_ADDR(bus, 0x1000);
    BUS_SET_DATA(bus, 0x48);  // PHA opcode
    cout << "Address: $" << hex << setw(4) << setfill('0') << BUS_GET_ADDR(bus) << "\n";
    cout << "Initial bus data: $" << hex << setw(2) << setfill('0') << (int)BUS_GET_DATA(bus) << "\n";
    
    bus = cpu.cycle_tick(bus);
    
    cout << "After cycle: bus data = $" << hex << setw(2) << setfill('0') << (int)BUS_GET_DATA(bus) << "\n";
    cout << "PC: $" << hex << setw(4) << setfill('0') << (int)cpu.get_pc() << "\n";
    cout << "Cycle step: " << (int)cpu.get_cycle_step() << "\n\n";
    
    // Cycle 2: Dummy read
    cout << "--- Cycle 2 ---\n";
    BUS_SET_ADDR(bus, 0x1001);
    BUS_SET_DATA(bus, 0xea);  // Dummy data
    cout << "Address: $" << hex << setw(4) << setfill('0') << BUS_GET_ADDR(bus) << "\n";
    cout << "Initial bus data: $" << hex << setw(2) << setfill('0') << (int)BUS_GET_DATA(bus) << "\n";
    
    bus = cpu.cycle_tick(bus);
    
    cout << "After cycle: bus data = $" << hex << setw(2) << setfill('0') << (int)BUS_GET_DATA(bus) << "\n";
    cout << "PC: $" << hex << setw(4) << setfill('0') << (int)cpu.get_pc() << "\n";
    cout << "Cycle step: " << (int)cpu.get_cycle_step() << "\n\n";
    
    // Cycle 3: Stack write
    cout << "--- Cycle 3 ---\n";
    BUS_SET_ADDR(bus, 0x01ff);
    BUS_SET_DATA(bus, 0x00);  // Initial data (will be overwritten by CPU)
    cout << "Address: $" << hex << setw(4) << setfill('0') << BUS_GET_ADDR(bus) << "\n";
    cout << "Initial bus data: $" << hex << setw(2) << setfill('0') << (int)BUS_GET_DATA(bus) << "\n";
    cout << "get_write_data() BEFORE cycle_tick(): $" << hex << setw(2) << setfill('0') << (int)cpu.get_write_data() << "\n";
    cout << "R/W line BEFORE: " << (cpu.get_rw() ? "READ" : "WRITE") << "\n";
    
    bus = cpu.cycle_tick(bus);
    
    cout << "After cycle: bus data = $" << hex << setw(2) << setfill('0') << (int)BUS_GET_DATA(bus) << "\n";
    cout << "get_write_data() AFTER cycle_tick(): $" << hex << setw(2) << setfill('0') << (int)cpu.get_write_data() << "\n";
    cout << "R/W line AFTER: " << (cpu.get_rw() ? "READ" : "WRITE") << "\n";
    cout << "PC: $" << hex << setw(4) << setfill('0') << (int)cpu.get_pc() << "\n";
    cout << "Cycle step: " << (int)cpu.get_cycle_step() << "\n";
    cout << "Final S: $" << hex << setw(2) << setfill('0') << (int)cpu.get_s() << "\n\n";
    
    return 0;
}