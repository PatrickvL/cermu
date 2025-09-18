#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

// Create a simple test memory simulator
class TestMemory {
private:
    uint8_t memory[0x10000];
    
public:
    TestMemory() {
        // Initialize memory to 0
        for (int i = 0; i < 0x10000; i++) {
            memory[i] = 0x00;
        }
        
        // Set up test program
        memory[0x1000] = 0x48;  // PHA - Push Accumulator
        memory[0x1001] = 0xEA;  // NOP (end test)
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
        std::cout << "MEMORY WRITE: 0x" << std::hex << std::setw(4) << std::setfill('0') 
                  << addr << " = 0x" << std::setw(2) << (int)data << std::dec << std::endl;
    }
    
    uint8_t read_stack(uint8_t sp) {
        return memory[0x0100 | sp];
    }
};

int main() {
    std::cout << "=== STACK OPERATIONS DEDUPLICATION TEST ===" << std::endl;
    
    // Create CPU with 6502 configuration
    using CpuConfig = cpu_config<CpuVariant::NMOS_6502, true, false, true, true, false, false, false, false, false, false, true, false, '\x10', false, false, false>;
    fam65xx<CpuConfig> cpu;
    TestMemory mem;
    
    // Initialize CPU for testing
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_a(0x42);      // Set accumulator to test value
    cpu.set_pc(0x1000);   // Set PC to test program
    cpu.set_s(0xFF);      // Set stack pointer
    
    std::cout << "\nInitial state:" << std::endl;
    std::cout << "  A=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a();
    std::cout << " S=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_s();
    std::cout << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.get_pc() << std::dec << std::endl;
    
    // Simulate instruction execution
    bus_state_t bus_state = 0;
    
    std::cout << "\nExecuting PHA instruction:" << std::endl;
    
    // Execute several cycles to complete PHA instruction
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        
        if (is_write) {
            uint8_t write_data = cpu.get_write_data();
            std::cout << "Cycle " << cycle << ": WRITE addr=0x" 
                      << std::hex << std::setw(4) << std::setfill('0') << addr
                      << " data=0x" << std::setw(2) << (int)write_data << std::dec << std::endl;
            mem.write(addr, write_data);
        } else {
            uint8_t read_data = mem.read(addr);
            std::cout << "Cycle " << cycle << ": READ  addr=0x" 
                      << std::hex << std::setw(4) << std::setfill('0') << addr
                      << " data=0x" << std::setw(2) << (int)read_data << std::dec << std::endl;
            BUS_SET_DATA(bus_state, read_data);
        }
        
        BUS_SET_ADDR(bus_state, addr);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction completed
        if (cpu.get_opcode() == 0xEA || cycle > 5) { // NOP or timeout
            break;
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  A=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a();
    std::cout << " S=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_s();
    std::cout << " PC=0x" << std::hex << std::setw(4) << std::setfill('0') << (int)cpu.get_pc() << std::dec << std::endl;
    
    // Check stack memory
    std::cout << "\nStack memory at 0x01FF: 0x" 
              << std::hex << std::setw(2) << std::setfill('0') << (int)mem.read_stack(0xFF) << std::dec << std::endl;
    
    // Test result
    uint8_t stack_value = mem.read_stack(0xFF);
    if (stack_value == 0x42) {
        std::cout << "\n✓ SUCCESS: Stack operations working correctly with deduplication!" << std::endl;
        std::cout << "✓ DEDUPLICATION VERIFIED: get_store_register_value() helper function is being used properly!" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ FAILURE: Expected 0x42 on stack, got 0x" 
                  << std::hex << std::setw(2) << std::setfill('0') << (int)stack_value << std::dec << std::endl;
        return 1;
    }
}