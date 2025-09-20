#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== Simple RTS Test ===\n";
    
    // Create CPU instance
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    cpu.init_for_test();
    
    // Test Case: RTS from subroutine
    // Initial state: PC=0x4000, SP=0xFC (stack has return address 0x3456-1)
    // Stack: [0x01FD]=0x55, [0x01FE]=0x34 (return address 0x3455)
    // Expected: PC=0x3456, SP=0xFE
    
    // Set up initial CPU state
    cpu.set_pc(0x4000);        // PC at RTS instruction
    cpu.set_s(0xFC);           // Stack pointer at 0xFC
    cpu.set_a(0x00);
    cpu.set_x(0x00);
    cpu.set_y(0x00);
    cpu.set_p(0x20);           // Normal flags
    
    std::cout << "Initial state:\n";
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << "\n";
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_s()) << "\n";
    std::cout << "  P: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_p()) << std::dec << "\n";
    
    // Create proper bus state using system_lines.h macros
    auto create_bus_state = [](uint16_t addr, uint8_t data) -> bus_state_t {
        bus_state_t state = 0;
        BUS_SET_ADDR(state, addr);
        BUS_SET_DATA(state, data);
        state |= BUS_BIT(BUS_RDY_BIT); // RDY high (ready)
        return state;
    };
    
    // Simulate external memory system for stack data
    auto get_memory_data = [](uint16_t addr) -> uint8_t {
        switch (addr) {
            case 0x4000: return 0x60;  // RTS opcode
            case 0x01FD: return 0x55;  // Low byte of return address-1
            case 0x01FE: return 0x34;  // High byte of return address-1
            default: return 0x00;      // Default memory value
        }
    };
    
    std::cout << "\nStack contents (simulated):\n";
    std::cout << "  [0x01FD]: 0x55 (low byte of return address-1)\n";
    std::cout << "  [0x01FE]: 0x34 (high byte of return address-1)\n";
    std::cout << "  Expected return address: 0x3456\n";
    
    // Execute RTS instruction through multiple cycles
    std::cout << "\n=== Executing RTS instruction ===\n";
    
    // Run until instruction completes
    int cycle = 0;
    uint16_t last_pc = cpu.get_pc();
    uint8_t last_sp = cpu.get_s();
    
    while (cycle < 10) { // Safety limit
        uint16_t addr = cpu.get_pc();
        if (cycle > 0) {
            // For cycles after opcode fetch, CPU may request different addresses
            addr = cpu.get_address(); // Get actual address being requested
        }
        
        uint8_t data = get_memory_data(addr);
        bus_state_t bus_state = create_bus_state(addr, data);
        
        std::cout << "Cycle " << cycle << ": ";
        std::cout << "Addr=0x" << std::hex << std::setw(4) << std::setfill('0') << addr;
        std::cout << ", Data=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data);
        std::cout << ", PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc();
        std::cout << ", SP=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_s());
        std::cout << ", Step=" << static_cast<int>(cpu.get_cycle_step());
        std::cout << std::dec << "\n";
        
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction completed (PC changed significantly or cycle completed)
        if (cpu.get_pc() != last_pc && cycle > 0) {
            std::cout << "Instruction completed! PC changed from 0x" << std::hex << last_pc
                      << " to 0x" << cpu.get_pc() << std::dec << "\n";
            break;
        }
        
        last_pc = cpu.get_pc();
        last_sp = cpu.get_s();
        cycle++;
    }
    
    // Check results
    uint16_t final_pc = cpu.get_pc();
    uint8_t final_sp = cpu.get_s();
    
    std::cout << "\nFinal state:\n";
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << final_pc << "\n";
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(final_sp) << "\n";
    std::cout << "  P: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_p()) << std::dec << "\n";
    
    // Expected results
    uint16_t expected_pc = 0x3456;  // Return address
    uint8_t expected_sp = 0xFE;     // SP after pulling 2 bytes
    
    std::cout << "\nExpected:\n";
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << expected_pc << "\n";
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(expected_sp) << std::dec << "\n";
    
    // Check if test passed
    bool pc_correct = (final_pc == expected_pc);
    bool sp_correct = (final_sp == expected_sp);
    bool test_passed = pc_correct && sp_correct;
    
    std::cout << "\n=== RESULTS ===\n";
    std::cout << "PC correct: " << (pc_correct ? "YES" : "NO") << "\n";
    std::cout << "SP correct: " << (sp_correct ? "YES" : "NO") << "\n";
    std::cout << "Test: " << (test_passed ? "PASSED" : "FAILED") << "\n";
    
    return test_passed ? 0 : 1;
}