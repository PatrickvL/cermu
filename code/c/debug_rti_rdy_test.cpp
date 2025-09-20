#include <cstdio>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

// Mock memory for RTI test
class MockMemory {
public:
    uint8_t memory[0x10000] = {0};
    
    MockMemory() {
        // Set up RTI test scenario
        memory[0x8000] = 0x40;  // RTI instruction
        
        // Set up stack with test data
        memory[0x01FE] = 0x24;  // Status register on stack
        memory[0x01FF] = 0xAA;  // PCL on stack
        memory[0x0100] = 0x65;  // PCH on stack
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    MockMemory mem;
    
    printf("=== RTI RDY LINE DEBUG TEST ===\n");
    
    // Initialize CPU for testing
    cpu.init_for_test();
    cpu.set_pc(0x8000);
    cpu.set_sp(0xFD);
    cpu.set_p(0x04);
    
    printf("Initial state: PC=0x%04X, SP=0x%02X, P=0x%02X\n", 
           cpu.get_pc(), cpu.get_sp(), cpu.get_p());
    
    // Test RTI execution with different RDY line states
    for (int cycle = 0; cycle < 10; cycle++) {
        printf("\n### RTI CYCLE %d ###\n", cycle);
        
        uint16_t addr = cpu.get_address();
        bool rw = cpu.get_rw();
        uint8_t write_data = rw ? 0 : cpu.get_write_data();
        
        printf("BEFORE: PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
               cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
        printf("Memory Access: addr=0x%04X, rw=%s, data=0x%02X\n", 
               addr, rw ? "READ" : "WRITE", rw ? mem.read(addr) : write_data);
        
        // Create bus state with RDY high (ready) - NO RDY blocking
        bus_state_t bus_state = 0;
        BUS_SET_ADDR(bus_state, addr);
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY = 1 (ready)
        
        if (rw) {
            bus_state = BUS_SET_DATA(bus_state, mem.read(addr));
        } else {
            mem.write(addr, write_data);
            bus_state = BUS_SET_DATA(bus_state, write_data);
        }
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        printf("AFTER:  PC=0x%04X, SP=0x%02X, P=0x%02X, Opcode=0x%02X, Step=%d\n",
               cpu.get_pc(), cpu.get_sp(), cpu.get_p(), cpu.get_opcode(), cpu.get_cycle_step());
        
        // Check if instruction completed
        if (cpu.get_cycle_step() == 0) {
            printf("RTI INSTRUCTION COMPLETED after %d cycles\n", cycle + 1);
            break;
        }
        
        // Error detection: RTI should not get stuck
        if (cycle > 0 && cpu.get_cycle_step() == 1 && cpu.get_opcode() == 0x40) {
            printf("ERROR: RTI stuck in cycle 1 - same RDY line issue as RTS!\n");
            break;
        }
    }
    
    printf("\n=== RTI TEST WITH RDY LOW (NOT READY) ===\n");
    
    // Reset CPU for second test
    cpu.init_for_test();
    cpu.set_pc(0x8000);
    cpu.set_sp(0xFD);
    cpu.set_p(0x04);
    
    // Test with RDY line low (not ready) - should block read cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("\n### RTI CYCLE %d (RDY LOW) ###\n", cycle);
        
        uint16_t addr = cpu.get_address();
        bool rw = cpu.get_rw();
        
        printf("Memory Access: addr=0x%04X, rw=%s\n", addr, rw ? "READ" : "WRITE");
        
        // Create bus state with RDY low (not ready) for read cycles
        bus_state_t bus_state = 0;
        BUS_SET_ADDR(bus_state, addr);
        
        if (rw && cycle > 0) {
            // RDY low for read cycles during instruction execution
            bus_state &= ~BUS_BIT(BUS_RDY_BIT);  // RDY = 0 (not ready)
            printf("RDY = 0 (not ready) - should block this read cycle\n");
        } else {
            // RDY high for opcode fetch and write cycles
            bus_state |= BUS_BIT(BUS_RDY_BIT);   // RDY = 1 (ready)
            printf("RDY = 1 (ready) - should proceed normally\n");
        }
        
        if (rw) {
            bus_state = BUS_SET_DATA(bus_state, mem.read(addr));
        }
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        printf("AFTER:  Opcode=0x%02X, Step=%d\n", cpu.get_opcode(), cpu.get_cycle_step());
        
        // Check for RDY blocking behavior
        if (rw && cycle > 0 && !(bus_state & BUS_BIT(BUS_RDY_BIT))) {
            printf("Read cycle should be blocked by RDY line\n");
        }
        
        if (cpu.get_cycle_step() == 0) break;
    }
    
    return 0;
}