#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

// Debug version of the test harness
class DebugTestHarness {
private:
    fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536];

public:
    DebugTestHarness() {
        std::fill(memory, memory + 65536, 0);
        cpu.init_for_test();
    }
    
    void set_pc(uint16_t pc) { cpu.set_pc(pc); }
    void set_a(uint8_t a) { cpu.set_a(a); }
    void set_sp(uint8_t sp) { cpu.set_sp(sp); }
    void set_memory(uint16_t addr, uint8_t data) { memory[addr] = data; }
    
    uint16_t get_pc() const { return cpu.get_pc(); }
    uint8_t get_a() const { return cpu.get_a(); }
    uint8_t get_sp() const { return cpu.get_sp(); }
    uint8_t get_memory(uint16_t addr) const { return memory[addr]; }
    uint32_t get_cycle_count() const { return cpu.get_cycle_count(); }
    
    // Debug step method with detailed logging
    bool step() {
        std::cout << "=== HARNESS STEP DEBUG ===" << std::endl;
        std::cout << "Initial PC: 0x" << std::hex << cpu.get_pc() << std::endl;
        std::cout << "Initial cycle_step: " << std::dec << (int)cpu.get_cycle_step() << std::endl;
        std::cout << "Initial opcode: 0x" << std::hex << cpu.get_opcode() << std::endl;
        
        uint32_t initial_cycles = cpu.get_cycle_count();
        
        for (int max_cycles = 0; max_cycles < 10; max_cycles++) {
            std::cout << "\n--- Cycle " << max_cycles << " ---" << std::endl;
            
            uint16_t addr = cpu.get_address();
            bool is_write = !cpu.get_rw();
            uint8_t step_before = cpu.get_cycle_step();
            uint16_t opcode_before = cpu.get_opcode();
            
            std::cout << "Before: PC=0x" << std::hex << cpu.get_pc() 
                      << ", step=" << std::dec << (int)step_before 
                      << ", opcode=0x" << std::hex << opcode_before << std::endl;
            std::cout << "Access: addr=0x" << std::hex << addr 
                      << ", " << (is_write ? "WRITE" : "READ") << std::endl;
            
            bus_state_t bus_state = 0;
            if (is_write) {
                uint8_t data = cpu.get_write_data();
                memory[addr] = data;
                BUS_SET_DATA(bus_state, data);
                std::cout << "Writing data=0x" << std::hex << (int)data << std::endl;
            } else {
                uint8_t data = memory[addr];
                BUS_SET_DATA(bus_state, data);
                std::cout << "Reading data=0x" << std::hex << (int)data << std::endl;
            }
            
            bus_state |= BUS_BIT(BUS_RDY_BIT);
            bus_state |= BUS_BIT(BUS_SO_BIT);
            
            // Execute cycle
            bus_state = cpu.cycle_tick(bus_state);
            
            uint8_t step_after = cpu.get_cycle_step();
            uint16_t opcode_after = cpu.get_opcode();
            
            std::cout << "After:  PC=0x" << std::hex << cpu.get_pc() 
                      << ", step=" << std::dec << (int)step_after 
                      << ", opcode=0x" << std::hex << opcode_after << std::endl;
            
            // Check completion condition
            if (step_after == 0 && max_cycles > 0) {
                std::cout << "*** INSTRUCTION COMPLETED after " << max_cycles + 1 << " cycles ***" << std::endl;
                std::cout << "Final cycle count: " << (cpu.get_cycle_count() - initial_cycles) << std::endl;
                return true;
            }
        }
        
        std::cout << "*** INSTRUCTION DID NOT COMPLETE after 10 cycles ***" << std::endl;
        return false;
    }
};

int main() {
    std::cout << "=== Test Harness Debug ===" << std::endl;
    
    DebugTestHarness harness;
    
    // Set up PHA test case
    harness.set_pc(0x1000);
    harness.set_a(0x42);
    harness.set_sp(0xFF);
    harness.set_memory(0x1000, 0x48); // PHA
    harness.set_memory(0x1001, 0xEA); // NOP (next instruction)
    
    std::cout << "Setup complete. Executing PHA instruction..." << std::endl;
    
    bool result = harness.step();
    
    std::cout << "\n=== FINAL STATE ===" << std::endl;
    std::cout << "Step result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "Final PC: 0x" << std::hex << harness.get_pc() << std::endl;
    std::cout << "Final A: 0x" << std::hex << (int)harness.get_a() << std::endl;
    std::cout << "Final SP: 0x" << std::hex << (int)harness.get_sp() << std::endl;
    std::cout << "Stack[0x01FF]: 0x" << std::hex << (int)harness.get_memory(0x01FF) << std::endl;
    
    return result ? 0 : 1;
}