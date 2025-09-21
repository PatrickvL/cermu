#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

using namespace fam65xx_cpp;

// Custom debug CPU class that adds tracing to execute_cycle
template<typename Config>
class debug_fam65xx : public fam65xx<Config> {
public:
    // Override execute_cycle to add debug tracing
    inline bus_state_t execute_cycle(bus_state_t bus_state) {
        std::cout << "  [TRACE] execute_cycle() START - Step=" << (int)this->get_cycle_step() << std::endl;
        
        // Call original execute_cycle and trace the result
        bus_state_t result = fam65xx<Config>::execute_cycle(bus_state);
        
        std::cout << "  [TRACE] execute_cycle() END - Step=" << (int)this->get_cycle_step() << std::endl;
        
        return result;
    }
};

int main() {
    std::cout << "=== Cycle Flow Debug ===" << std::endl;
    
    // Create debug CPU instance
    debug_fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up initial state
    cpu.set_pc(0x1000);
    cpu.set_a(0x42);
    cpu.set_s(0xFF);
    
    // Memory simulation: PHA at 0x1000
    uint8_t memory[65536];
    memory[0x1000] = 0x48;  // PHA
    memory[0x1001] = 0xEA;  // NOP (next instruction)
    
    bus_state_t bus_state = 0;
    
    std::cout << "\n--- Tracing PHA Execution ---" << std::endl;
    
    // Cycle 0: Opcode fetch
    std::cout << "\n=== CYCLE 0: Opcode Fetch ===" << std::endl;
    uint16_t addr = cpu.get_address();
    uint8_t bus_data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, bus_data);
    std::cout << "Before cycle_tick: Step=" << (int)cpu.get_cycle_step() << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle_tick: Step=" << (int)cpu.get_cycle_step() << std::endl;
    
    // Cycle 1: Should advance to step 2
    std::cout << "\n=== CYCLE 1: Should advance step ===" << std::endl;
    addr = cpu.get_address();
    bus_data = memory[addr];
    bus_state = BUS_SET_DATA(bus_state, bus_data);
    std::cout << "Before cycle_tick: Step=" << (int)cpu.get_cycle_step() << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    std::cout << "After cycle_tick: Step=" << (int)cpu.get_cycle_step() << std::endl;
    
    return 0;
}