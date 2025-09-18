#include <iostream>
#include <iomanip>
#include <cassert>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace std;

int main() {
    cout << "=== Stack Cycle Debugging ===" << endl;
    
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_a(0x42);
    cpu.set_p(0xC3);
    cpu.set_s(0xFF);
    cpu.set_pc(0x1000);
    
    // Set up memory with PHA instruction
    uint8_t memory[65536] = {0};
    memory[0x1000] = 0x48;  // PHA opcode
    
    cout << "Initial state:" << endl;
    cout << "  A=0x" << hex << setw(2) << setfill('0') << (int)cpu.get_a();
    cout << " P=0x" << hex << setw(2) << setfill('0') << (int)cpu.get_p();
    cout << " S=0x" << hex << setw(2) << setfill('0') << (int)cpu.get_s();
    cout << " PC=0x" << hex << setw(4) << setfill('0') << (int)cpu.get_pc() << endl;
    
    cout << "\nExecuting PHA instruction (0x48):" << endl;
    
    // Execute cycles with detailed debugging
    for (int cycle = 0; cycle < 5; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        uint8_t write_data = 0;
        
        if (is_write) {
            write_data = cpu.get_write_data();
        }
        
        cout << "Cycle " << (cycle + 1) << ":" << endl;
        cout << "  Address: 0x" << hex << setw(4) << setfill('0') << addr;
        cout << "  R/W: " << (is_write ? "WRITE" : "READ");
        if (is_write) {
            cout << "  Data: 0x" << hex << setw(2) << setfill('0') << (int)write_data;
        }
        cout << "  Step: " << (int)cpu.get_cycle_step() << endl;
        
        // Create bus state
        bus_state_t bus_state = 0;
        if (is_write) {
            memory[addr] = write_data;
            BUS_SET_DATA(bus_state, write_data);
        } else {
            uint8_t data = memory[addr];
            BUS_SET_DATA(bus_state, data);
        }
        
        // Set RDY line
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        // Execute CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        cout << "  After cycle - Step: " << (int)cpu.get_cycle_step();
        cout << "  S=0x" << hex << setw(2) << setfill('0') << (int)cpu.get_s();
        cout << "  PC=0x" << hex << setw(4) << setfill('0') << (int)cpu.get_pc() << endl;
        
        // Check if instruction completed
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            cout << "Instruction completed!" << endl;
            break;
        }
    }
    
    cout << "\nFinal state:" << endl;
    cout << "  A=0x" << hex << setw(2) << setfill('0') << (int)cpu.get_a();
    cout << " P=0x" << hex << setw(2) << setfill('0') << (int)cpu.get_p();
    cout << " S=0x" << hex << setw(2) << setfill('0') << (int)cpu.get_s();
    cout << " PC=0x" << hex << setw(4) << setfill('0') << (int)cpu.get_pc() << endl;
    
    cout << "\nStack memory check:" << endl;
    cout << "  Stack[0x01FF]=0x" << hex << setw(2) << setfill('0') << (int)memory[0x01FF] << endl;
    cout << "  Stack[0x01FE]=0x" << hex << setw(2) << setfill('0') << (int)memory[0x01FE] << endl;
    
    return 0;
}