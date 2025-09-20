#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

class TestMemory {
private:
    uint8_t memory[0x10000];
    
public:
    TestMemory() {
        for (int i = 0; i < 0x10000; i++) {
            memory[i] = 0x00;
        }
        
        // Set up RTS test scenario
        memory[0x4000] = 0x60;  // RTS instruction
        
        // Stack setup: return address 0x3455 (so RTS returns to 0x3456)
        memory[0x01FD] = 0x55;  // Low byte at SP+1
        memory[0x01FE] = 0x34;  // High byte at SP+2
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    std::cout << "=== RTS Completion Logic Debug ===" << std::endl;
    
    TestMemory memory;
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_pc(0x4000);
    cpu.set_sp(0xFC);  // Stack pointer before pulls
    
    std::cout << "\nInitial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  Step: " << (int)cpu.get_cycle_step() << std::endl;
    
    // Execute cycles and track completion logic
    for (int cycle = 1; cycle <= 10; cycle++) {
        std::cout << "\nCycle " << cycle << ":" << std::endl;
        
        // Get current state before cycle
        uint16_t addr = cpu.get_address();
        bool rw = cpu.get_rw();
        uint8_t step_before = cpu.get_cycle_step();
        uint16_t opcode = cpu.get_opcode();
        
        std::cout << "  Before: Step=" << (int)step_before 
                  << ", Opcode=0x" << std::hex << opcode
                  << ", Addr=0x" << addr
                  << ", RW=" << (rw ? "READ" : "WRITE") << std::endl;
        
        // Create bus state
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_ADDR(bus_state, addr);
        if (rw) {
            bus_state |= BUS_BIT(BUS_RW_BIT);
        } else {
            bus_state &= ~BUS_BIT(BUS_RW_BIT);
        }
        
        // Set data for reads
        if (rw) {
            uint8_t data = memory.read(addr);
            bus_state = BUS_SET_DATA(bus_state, data);
            std::cout << "  Memory READ: Address=0x" << std::hex << addr 
                      << " Data=0x" << (int)data << std::endl;
        } else {
            uint8_t data = cpu.get_write_data();
            memory.write(addr, data);
            std::cout << "  Memory WRITE: Address=0x" << std::hex << addr 
                      << " Data=0x" << (int)data << std::endl;
        }
        
        // Execute cycle
        cpu.cycle_tick(bus_state);
        
        // Get state after cycle
        uint8_t step_after = cpu.get_cycle_step();
        uint16_t pc_after = cpu.get_pc();
        uint8_t sp_after = cpu.get_sp();
        
        std::cout << "  After: Step=" << (int)step_after
                  << ", PC=0x" << std::hex << pc_after
                  << ", SP=0x" << (int)sp_after << std::endl;
        
        // Check if RTS cycle table reports completion
        if (step_before > 0) {
            auto cycle_desc = fam65xx_cpp::fam65xx<Config>::GET_CYCLE(0x60, step_before);
            bool is_sync = cycle_desc.is_sync();
            std::cout << "  Cycle table sync flag for step " << (int)step_before << ": " 
                      << (is_sync ? "YES" : "NO") << std::endl;
        }
        
        // Check if instruction completed (step reset to 0)
        if (step_after == 0 && step_before > 0) {
            std::cout << "  *** INSTRUCTION COMPLETED ***" << std::endl;
            break;
        }
        
        // Stop if we're clearly stuck
        if (cycle > 1 && step_after == 1 && step_before == 1) {
            std::cout << "  *** INSTRUCTION STUCK AT STEP 1 ***" << std::endl;
            break;
        }
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_sp() << std::endl;
    std::cout << "  Step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << "  Expected PC: 0x3456" << std::endl;
    std::cout << "  Expected SP: 0xFE" << std::endl;
    
    return 0;
}