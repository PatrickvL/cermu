#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

// Mock bus state for testing
bus_state_t BUS_MEMORY[0x10000];

bus_state_t read_memory(uint16_t addr) {
    return BUS_MEMORY[addr];
}

void write_memory(uint16_t addr, uint8_t data) {
    BUS_MEMORY[addr] = data;
}

int main() {
    std::cout << "=== RTI Flag Restoration Debug Test ===" << std::endl;
    
    // Use NMOS 6502 configuration
    using TestConfig = cpu_config<CpuVariant::NMOS_6502>;
    fam65xx<TestConfig> cpu;
    
    // Initialize CPU for testing
    cpu.init_for_test();
    cpu.set_pc(0x8000);
    cpu.set_s(0xFC);   // SP will be incremented during RTI
    cpu.set_p(0x24);   // Initial P register (I=1, U=1)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "  SP: 0x" << std::hex << (int)cpu.get_s() << std::endl;
    std::cout << "  P:  0x" << std::hex << (int)cpu.get_p() << std::endl;
    
    // Set up memory
    BUS_MEMORY[0x8000] = 0x40;  // RTI opcode
    BUS_MEMORY[0x8001] = 0x00;  // Dummy byte
    
    // Test different flag combinations on stack
    struct test_case {
        uint8_t stack_flags;
        uint8_t expected_result;
        const char* description;
    };
    
    test_case tests[] = {
        {0x30, 0x20, "Stack=0x30 (U+B flags) -> Expected=0x20 (U flag only, B cleared)"},
        {0x20, 0x20, "Stack=0x20 (U flag only) -> Expected=0x20 (U flag preserved)"},
        {0x10, 0x00, "Stack=0x10 (B flag only) -> Expected=0x00 (B flag cleared)"},
        {0x00, 0x00, "Stack=0x00 (no flags) -> Expected=0x00 (no change)"},
        {0xFF, 0xEF, "Stack=0xFF (all flags) -> Expected=0xEF (all except B)"},
        {0x35, 0x25, "Stack=0x35 (N+U+B+C) -> Expected=0x25 (N+U+C, B cleared)"}
    };
    
    for (const auto& test : tests) {
        std::cout << "\n" << test.description << std::endl;
        
        // Reset CPU state
        cpu.set_pc(0x8000);
        cpu.set_s(0xFC);
        
        // Set up stack with test flags
        BUS_MEMORY[0x01FD] = test.stack_flags;  // Status register on stack
        BUS_MEMORY[0x01FE] = 0x34;               // PC low on stack
        BUS_MEMORY[0x01FF] = 0x12;               // PC high on stack
        
        std::cout << "  Stack flags: 0x" << std::hex << (int)test.stack_flags << std::endl;
        
        // Execute RTI instruction
        for (int cycle = 0; cycle < 6; cycle++) {
            uint16_t addr = cpu.get_address();
            uint8_t data = read_memory(addr);
            
            // Create bus state
            bus_state_t bus_state = 0;
            bus_state = BUS_SET_ADDRESS(bus_state, addr);
            bus_state = BUS_SET_DATA(bus_state, data);
            bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY line ready
            
            // Execute cycle
            bus_state = cpu.cycle_tick(bus_state);
            
            std::cout << "    Cycle " << cycle << ": Addr=0x" << std::hex << addr 
                      << " Data=0x" << std::hex << (int)data << std::endl;
        }
        
        uint8_t final_p = cpu.get_p();
        std::cout << "  Final P: 0x" << std::hex << (int)final_p << std::endl;
        std::cout << "  Expected: 0x" << std::hex << (int)test.expected_result << std::endl;
        std::cout << "  Result: " << ((final_p == test.expected_result) ? "✓ PASS" : "✗ FAIL") << std::endl;
    }
    
    return 0;
}