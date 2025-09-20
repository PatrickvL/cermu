#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

class DebugMemory {
private:
    uint8_t memory[0x10000];
    
public:
    DebugMemory() {
        // Initialize memory to 0
        for (int i = 0; i < 0x10000; i++) {
            memory[i] = 0x00;
        }
        
        // Set up stack with return address: 0x34 (high) at 0x01FE, 0x55 (low) at 0x01FD
        memory[0x01FD] = 0x55;  // Low byte of return address
        memory[0x01FE] = 0x34;  // High byte of return address
        
        // Place RTS instruction at 0x4000
        memory[0x4000] = 0x60;  // RTS opcode
    }
    
    uint8_t read(uint16_t address) {
        uint8_t data = memory[address];
        std::cout << "  Memory READ: Address=0x" << std::hex << std::setw(4) << std::setfill('0') 
                  << address << " Data=0x" << std::setw(2) << std::setfill('0') << (int)data << std::endl;
        return data;
    }
    
    void write(uint16_t address, uint8_t data) {
        std::cout << "  Memory WRITE: Address=0x" << std::hex << std::setw(4) << std::setfill('0') 
                  << address << " Data=0x" << std::setw(2) << std::setfill('0') << (int)data << std::endl;
        memory[address] = data;
    }
};

int main() {
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    DebugMemory memory;
    
    std::cout << "=== RTS Memory Address Debug ===" << std::endl;
    
    // Initialize CPU for test
    cpu.init_for_test();
    cpu.set_pc(0x4000);    // Start at RTS instruction
    cpu.set_sp(0xFC);      // Stack pointer before RTS (points to 0x01FC, will read from 0x01FD and 0x01FE)
    
    std::cout << "\nInitial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_sp() << std::endl;
    std::cout << "  Stack[0x01FD] = 0x55 (expected low byte)" << std::endl;
    std::cout << "  Stack[0x01FE] = 0x34 (expected high byte)" << std::endl;
    
    // Execute RTS instruction cycle by cycle
    bus_state_t bus_state = 0;
    int cycle_count = 0;
    
    while (cycle_count < 10) {  // Safety limit
        cycle_count++;
        
        // Get expected address and read/write state BEFORE cycle execution
        uint16_t expected_addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        uint8_t write_data = cpu.get_write_data();
        
        std::cout << "\nCycle " << std::dec << cycle_count << ":" << std::endl;
        std::cout << "  Step=" << (int)cpu.get_cycle_step() 
                  << ", Opcode=0x" << std::hex << std::setw(2) << std::setfill('0') << cpu.get_opcode()
                  << ", Expected_Addr=0x" << std::hex << std::setw(4) << std::setfill('0') << expected_addr
                  << ", RW=" << (is_read ? "READ" : "WRITE") << std::endl;
        
        // Set up bus data for the expected address
        if (is_read) {
            uint8_t data = memory.read(expected_addr);
            bus_state = 0;
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            std::cout << "  Write Data=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)write_data << std::endl;
            memory.write(expected_addr, write_data);
            bus_state = 0;
            bus_state = BUS_SET_DATA(bus_state, write_data);
        }
        
        // Execute the cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Show state after execution
        std::cout << "  After: PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc()
                  << ", SP=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_sp()
                  << ", Step=" << std::dec << (int)cpu.get_cycle_step() << std::endl;
        
        // Check if instruction completed (step went back to 0)
        if (cpu.get_cycle_step() == 0 && cycle_count > 1) {
            std::cout << "\nInstruction completed!" << std::endl;
            break;
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_sp() << std::endl;
    std::cout << "  Expected PC: 0x3456" << std::endl;
    std::cout << "  Expected SP: 0xFE" << std::endl;
    
    // Check results
    bool pc_correct = (cpu.get_pc() == 0x3456);
    bool sp_correct = (cpu.get_sp() == 0xFE);
    
    std::cout << "\nTest result: " << (pc_correct && sp_correct ? "PASS" : "FAIL") << std::endl;
    std::cout << "PC correct: " << (pc_correct ? "YES" : "NO") << std::endl;
    std::cout << "SP correct: " << (sp_correct ? "YES" : "NO") << std::endl;
    
    return 0;
}