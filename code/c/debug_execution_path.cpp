#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>

int main() {
    using Config = config_6502;
    using CPU = fam65xx_cpp::fam65xx_with_cycle_count<Config>;
    
    CPU cpu;
    cpu.init_for_test();
    
    std::cout << "=== Debugging Execution Path Selection ===" << std::endl;
    
    // Test what state flags are set initially
    std::cout << "Initial state flags: 0x" << std::hex << cpu.get_state_flags() << std::dec << std::endl;
    
    // Check individual state flags
    std::cout << "STATE_INTERRUPT_SEQUENCE: " << ((cpu.get_state_flags() & 0x0800) ? "SET" : "CLEAR") << std::endl;
    std::cout << "STATE_RESET_PENDING: " << ((cpu.get_state_flags() & 0x0001) ? "SET" : "CLEAR") << std::endl;
    std::cout << "STATE_NMI_PENDING: " << ((cpu.get_state_flags() & 0x0080) ? "SET" : "CLEAR") << std::endl;
    std::cout << "STATE_IRQ_PENDING: " << ((cpu.get_state_flags() & 0x0100) ? "SET" : "CLEAR") << std::endl;
    std::cout << "STATE_ABORT_PENDING: " << ((cpu.get_state_flags() & 0x10000) ? "SET" : "CLEAR") << std::endl;
    std::cout << "STATE_COP_PENDING: " << ((cpu.get_state_flags() & 0x20000) ? "SET" : "CLEAR") << std::endl;
    
    // Check the combined mask that determines fast path
    uint32_t critical_flags = cpu.get_state_flags() & (0x0800 | 0x0001 | 0x0080 | 0x0100 | 0x10000 | 0x20000);
    std::cout << "Critical flags mask: 0x" << std::hex << critical_flags << std::dec << std::endl;
    std::cout << "Fast path will be taken: " << (critical_flags == 0 ? "YES" : "NO") << std::endl;
    
    return 0;
}