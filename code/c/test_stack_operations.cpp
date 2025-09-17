#include <iostream>
#include <iomanip>
#include <cassert>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using namespace std;

class StackTestHarness {
private:
    fam65xx_cpp::fam65xx<config_6502>* cpu;
    uint8_t memory[65536];

public:
    StackTestHarness() {
        cpu = new fam65xx_cpp::fam65xx<config_6502>();
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
    }
    
    ~StackTestHarness() {
        delete cpu;
    }

    void write_memory(uint16_t addr, uint8_t data) { memory[addr] = data; }
    uint8_t read_memory(uint16_t addr) { return memory[addr]; }
    void reset() { cpu->init_for_test(); }
    void set_accumulator(uint8_t val) { cpu->set_a(val); }
    void set_status_register(uint8_t val) { cpu->set_p(val); }
    void set_stack_pointer(uint8_t val) { cpu->set_s(val); }
    void set_program_counter(uint16_t val) { cpu->set_pc(val); }
    uint8_t get_accumulator() { return cpu->get_a(); }
    uint8_t get_status_register() { return cpu->get_p(); }
    uint8_t get_stack_pointer() { return cpu->get_s(); }
    uint16_t get_program_counter() { return cpu->get_pc(); }
    
    void execute_instruction() {
        bus_state_t bus = 0;
        int cycles = 0;
        do {
            uint16_t addr = cpu->get_address();
            BUS_SET_ADDR(bus, addr);
            BUS_SET_DATA(bus, read_memory(addr));  // Always provide data first
            
            // Let CPU execute the cycle
            cpu->cycle_tick(bus);
            
            // Check if CPU wants to write (R/W line = 0)
            uint8_t lines = BUS_GET_LINES(bus);
            if (!(lines & BUS_MASK_RW)) {
                // Write operation: save CPU's data to memory
                uint8_t data = BUS_GET_DATA(bus);
                write_memory(addr, data);
                // Debug output to see what's being written
                if (cycles > 1) {  // Skip instruction fetch cycles
                    cout << "    DEBUG: Writing 0x" << hex << setw(2) << setfill('0') << (int)data
                         << " to address 0x" << hex << setw(4) << setfill('0') << addr << endl;
                }
            }
            
            cycles++;
        } while (cpu->get_cycle_step() != 0 && cycles < 10);
    }

    void test_stack_instruction(const string& name, uint8_t opcode, const string& description) {
        cout << "Test: " << name << " - " << description << endl;
        
        reset();
        
        // Set up initial state
        set_accumulator(0x42);
        set_status_register(0xC3); // N=1, V=1, B=1, I=1, Z=0, C=1
        set_stack_pointer(0xFF);   // Start with full stack
        set_program_counter(0x1000);
        
        // Set up memory
        write_memory(0x1000, opcode);
        
        cout << "  Initial: A=0x" << hex << setw(2) << setfill('0') << (int)get_accumulator()
             << " P=0x" << hex << setw(2) << setfill('0') << (int)get_status_register()
             << " S=0x" << hex << setw(2) << setfill('0') << (int)get_stack_pointer()
             << " PC=0x" << hex << setw(4) << setfill('0') << (int)get_program_counter() << endl;
        
        // Execute instruction
        execute_instruction();
        
        cout << "  Final:   A=0x" << hex << setw(2) << setfill('0') << (int)get_accumulator()
             << " P=0x" << hex << setw(2) << setfill('0') << (int)get_status_register()
             << " S=0x" << hex << setw(2) << setfill('0') << (int)get_stack_pointer()
             << " PC=0x" << hex << setw(4) << setfill('0') << (int)get_program_counter() << endl;
        
        // Check stack memory for push operations
        if (name == "PHA" || name == "PHP") {
            cout << "  Stack[0x01FF]=0x" << hex << setw(2) << setfill('0') << (int)read_memory(0x01FF) << endl;
        }
        
        cout << endl;
    }
};

int main() {
    cout << "=== Stack Operations Test ===" << endl;
    
    StackTestHarness test;
    
    // Test all 4 stack operations
    test.test_stack_instruction("PHA", 0x48, "Push Accumulator");
    test.test_stack_instruction("PHP", 0x08, "Push Processor Status"); 
    test.test_stack_instruction("PLA", 0x68, "Pull Accumulator");
    test.test_stack_instruction("PLP", 0x28, "Pull Processor Status");
    
    cout << "=== Stack Tests Complete ===" << endl;
    cout << "🔧 Next: Analyze results and fix stack operation implementation" << endl;
    
    return 0;
}