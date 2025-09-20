#include <iostream>
#include <cstdio>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace fam65xx_cpp;

// Custom CPU class that traces P register changes
class TracingCpu : public fam65xx<config_6502> {
public:
    uint8_t prev_p = 0;
    
    void trace_p_change(const char* location) {
        uint8_t current_p = get_p();
        if (current_p != prev_p) {
            printf("P CHANGE at %s: 0x%02X -> 0x%02X (changed bits: 0x%02X)\n", 
                   location, prev_p, current_p, prev_p ^ current_p);
            
            // Specifically check for overflow flag changes
            bool prev_v = (prev_p & P_OVERFLOW) != 0;
            bool curr_v = (current_p & P_OVERFLOW) != 0;
            if (prev_v != curr_v) {
                printf("  *** OVERFLOW FLAG CHANGED: %s -> %s ***\n", 
                       prev_v ? "SET" : "CLEAR", curr_v ? "SET" : "CLEAR");
            }
            prev_p = current_p;
        }
    }
    
    // Override cycle_tick to trace P register changes
    bus_state_t cycle_tick(bus_state_t bus_state) {
        printf("\n=== CYCLE START: step=%d ===\n", get_cycle_step());
        trace_p_change("cycle_start");
        
        bus_state_t result = fam65xx<config_6502>::cycle_tick(bus_state);
        
        trace_p_change("cycle_end");
        printf("=== CYCLE END ===\n");
        
        return result;
    }
};

// Simple memory interface for debugging
class DebugMemory {
private:
    uint8_t memory[65536];
    
public:
    DebugMemory() {
        // Initialize all memory to 0
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
    }
    
    uint8_t read(uint16_t addr) const {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    std::cout << "=== RTI Flag Tracing Debug ===\n";
    
    // Create CPU instance
    TracingCpu cpu;
    cpu.init_for_test();
    
    // Create memory
    DebugMemory memory;
    
    // Set up a simple RTI test case
    // Place RTI instruction at 0x8000
    memory.write(0x8000, 0x40);  // RTI opcode
    
    // Set up stack with test values
    cpu.set_s(0xFC);  // Stack pointer (will be incremented during RTI)
    memory.write(0x01FD, 0x30);  // Status register on stack
    memory.write(0x01FE, 0x34);  // PC low byte on stack  
    memory.write(0x01FF, 0x12);  // PC high byte on stack
    
    // Set initial state
    cpu.set_pc(0x8000);
    cpu.set_p(0x24);  // Some initial flags
    cpu.prev_p = 0x24;  // Initialize trace
    
    std::cout << "Initial state:\n";
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_s() << std::endl;
    std::cout << "  P:  0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Execute RTI instruction cycle by cycle
    std::cout << "\nExecuting RTI instruction:\n";
    
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t addr = cpu.get_address();
        bool is_write = !cpu.get_rw();
        uint8_t write_data = is_write ? cpu.get_write_data() : 0;
        
        std::cout << "Cycle " << cycle << ": addr=0x" << std::hex << addr 
                  << " " << (is_write ? "WRITE" : "READ") 
                  << " opcode=0x" << std::hex << cpu.get_opcode()
                  << " step=" << std::dec << (int)cpu.get_cycle_step();
        
        if (is_write) {
            std::cout << " data=0x" << std::hex << (int)write_data;
            memory.write(addr, write_data);
        }
        std::cout << std::endl;
        
        // Create bus state with memory data
        bus_state_t bus_state = 0;
        if (!is_write) {
            uint8_t read_data = memory.read(addr);
            bus_state = BUS_SET_DATA(bus_state, read_data);
            std::cout << "  Read data: 0x" << std::hex << (int)read_data << std::endl;
        }
        
        // Set RDY line high (ready)
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0) {
            std::cout << "  *** Instruction complete ***\n";
            break;
        }
    }
    
    std::cout << "\nFinal state:\n";
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_s() << std::endl;
    std::cout << "  P:  0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    return 0;
}