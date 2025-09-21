#include "tests/fam65xx_cpp_test_harness.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== Stack Operations Debug Analysis ===" << std::endl;
    
    // Create CPU using the correct interface from test harness
    auto cpu = new fam65xx_cpp::fam65xx<config_6502>();
    auto memory = new TestMemory();
    
    if (!cpu || !memory) {
        std::cerr << "Failed to create CPU or memory" << std::endl;
        return 1;
    }
    
    std::cout << "\n--- Testing PHA (0x48) ---" << std::endl;
    
    // Initialize CPU
    cpu->init();
    
    // Set up a simple test scenario
    cpu->set_a(0x42);           // Set A register to 0x42
    cpu->set_s(0xFF);           // Set stack pointer to 0xFF
    cpu->set_pc(0x1000);        // Set PC to 0x1000
    
    // Setup memory with PHA instruction
    memory->write(0x1000, 0x48); // PHA instruction
    memory->write(0x1001, 0xEA); // NOP instruction after
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  A: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu->get_a() << std::endl;
    std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu->get_s() << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
              << cpu->get_pc() << std::endl;
    
    // Execute PHA instruction cycle by cycle
    std::cout << "\nExecuting PHA cycles:" << std::endl;
    
    int cycle_count = 0;
    while (cycle_count < 10) { // Safety limit
        cycle_count++;
        
        uint16_t old_pc = cpu->get_pc();
        uint8_t old_s = cpu->get_s();
        
        // Execute one CPU cycle with proper bus handling
        bus_state_t bus_state = 0;
        bus_state |= BUS_BIT(BUS_RW_BIT);  // Default to read
        bus_state |= BUS_BIT(BUS_RDY_BIT); // CPU is ready
        
        bus_state = cpu->cycle_tick(bus_state);
        
        // Handle memory operations
        if (bus_state & BUS_BIT(BUS_RW_BIT)) {
            // Read cycle
            uint16_t addr = BUS_GET_ADDR(bus_state);
            uint8_t data = memory->read(addr);
            BUS_SET_DATA(bus_state, data);
        } else {
            // Write cycle
            uint16_t addr = BUS_GET_ADDR(bus_state);
            uint8_t write_data = BUS_GET_DATA(bus_state);
            memory->write(addr, write_data);
            
            std::cout << "  Write: [0x" << std::hex << std::setw(4) << std::setfill('0') 
                      << addr << "] = 0x" << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)write_data << std::endl;
        }
        
        uint16_t new_pc = cpu->get_pc();
        uint8_t new_s = cpu->get_s();
        
        std::cout << "Cycle " << cycle_count << ":" << std::endl;
        std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << old_pc 
                  << " -> 0x" << std::hex << std::setw(4) << std::setfill('0') << new_pc << std::endl;
        std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)old_s 
                  << " -> 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)new_s << std::endl;
        
        // Check if instruction is complete (PC moved to next instruction)
        if (new_pc != old_pc && new_pc == 0x1001) {
            std::cout << "PHA instruction completed!" << std::endl;
            break;
        }
    }
    
    // Check final results
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  A: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu->get_a() << std::endl;
    std::cout << "  S: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu->get_s() << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
              << cpu->get_pc() << std::endl;
    
    // Check what was written to stack
    uint8_t stack_value = memory->read(0x01FF);
    std::cout << "  Stack[0x01FF]: 0x" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)stack_value << std::endl;
    
    std::cout << "\nExpected:" << std::endl;
    std::cout << "  A: 0x42 (unchanged)" << std::endl;
    std::cout << "  S: 0xFE (decremented)" << std::endl;
    std::cout << "  PC: 0x1001 (incremented)" << std::endl;
    std::cout << "  Stack[0x01FF]: 0x42 (A value)" << std::endl;
    
    // Cleanup
    delete memory;
    delete cpu;
    
    return 0;
}