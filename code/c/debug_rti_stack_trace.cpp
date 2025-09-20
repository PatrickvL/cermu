#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

// Enhanced tracing CPU class to monitor stack operations
class StackTracingCpu : public fam65xx_cpp::fam65xx<config_6502> {
public:
    void init_test() {
        init_for_test();
    }
    
    // Override handle_stack_pull to trace RTI stack operations
    void trace_stack_pull(uint8_t data) {
        std::cout << "STACK_PULL: cycle=" << static_cast<int>(get_cycle_step())
                  << ", data=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data)
                  << ", SP=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(get_s())
                  << std::endl;
        
        if (get_opcode() == 0x40) { // RTI
            switch (get_cycle_step()) {
                case 3:
                    std::cout << "  RTI Cycle 3: Pulling status register P=0x"
                              << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data) << std::endl;
                    std::cout << "  Before P=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(get_p()) << std::endl;
                    set_reg(CpuReg::P, data);
                    std::cout << "  After P=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(get_p()) << std::endl;
                    break;
                case 4:
                    std::cout << "  RTI Cycle 4: Pulling PCL=0x"
                              << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data) << std::endl;
                    std::cout << "  Before PC=0x" << std::hex << std::setw(4) << std::setfill('0') << get_pc() << std::endl;
                    set_reg(CpuReg::PCL, data);
                    std::cout << "  After PC=0x" << std::hex << std::setw(4) << std::setfill('0') << get_pc() << std::endl;
                    break;
                case 5:
                    std::cout << "  RTI Cycle 5: Pulling PCH=0x"
                              << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data) << std::endl;
                    std::cout << "  Before PC=0x" << std::hex << std::setw(4) << std::setfill('0') << get_pc() << std::endl;
                    set_reg(CpuReg::PCH, data);
                    std::cout << "  After PC=0x" << std::hex << std::setw(4) << std::setfill('0') << get_pc() << std::endl;
                    break;
            }
        }
    }
    
    // Override cycle_tick to trace stack operations
    bus_state_t cycle_tick(bus_state_t bus_state) {
        // Call parent cycle_tick
        auto result = fam65xx_cpp::fam65xx<config_6502>::cycle_tick(bus_state);
        
        // If this was a stack pull cycle, trace it
        if (get_opcode() == 0x40 && get_cycle_step() >= 3 && get_cycle_step() <= 5) {
            uint8_t data = BUS_GET_DATA(bus_state);
            trace_stack_pull(data);
        }
        
        return result;
    }
};

int main() {
    std::cout << "=== RTI Stack Operations Trace Test ===" << std::endl;
    
    StackTracingCpu cpu;
    cpu.init_test();
    
    // Set up a simple RTI test case similar to ProcessorTests
    // Test case: "40 9c 2c" from ProcessorTests
    std::cout << "\n--- Test Case: RTI with complex stack state ---" << std::endl;
    
    // Set initial state to match ProcessorTests failure case
    cpu.set_pc(0x8771);  // Initial PC where RTI instruction is located
    cpu.set_s(0xFC);     // Stack pointer (before RTI pulls 3 bytes, will increment to 0xFF)
    cpu.set_p(0x9C);     // Initial processor status
    
    // Set up memory simulation - stack contains the values that should be pulled
    // Stack layout (from top to bottom after RTI):
    // 0x01FD: Status register value (what P should become)
    // 0x01FE: PCL (low byte of return address)  
    // 0x01FF: PCH (high byte of return address)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  SP=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_s()) << std::endl;
    std::cout << "  P=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_p()) << std::endl;
    
    // Simulate stack memory for RTI pulls
    uint8_t stack_memory[3];
    
    // Expected values from ProcessorTests failure case
    // Expected final P=0xac, PC=0x65aa
    stack_memory[0] = 0xAC;  // Status register (will be pulled in cycle 3)
    stack_memory[1] = 0xAA;  // PCL (will be pulled in cycle 4) 
    stack_memory[2] = 0x65;  // PCH (will be pulled in cycle 5)
    
    std::cout << "\nStack memory setup:" << std::endl;
    std::cout << "  Stack[SP+1]=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(stack_memory[0]) << " (P)" << std::endl;
    std::cout << "  Stack[SP+2]=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(stack_memory[1]) << " (PCL)" << std::endl;
    std::cout << "  Stack[SP+3]=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(stack_memory[2]) << " (PCH)" << std::endl;
    
    // Execute RTI instruction cycle by cycle
    std::cout << "\n--- Executing RTI instruction ---" << std::endl;
    
    // Create bus state for opcode fetch
    bus_state_t bus_state = 0;
    
    // Cycle 0: Opcode fetch (RTI = 0x40)
    std::cout << "\nCycle 0: Opcode fetch" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, 0x40);
    bus_state = cpu.cycle_tick(bus_state);
    std::cout << "  After opcode fetch: opcode=0x" << std::hex << std::setw(2) << std::setfill('0') << cpu.get_opcode() << std::endl;
    
    // Cycle 1: Internal operation
    std::cout << "\nCycle 1: Internal operation" << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 2: Stack increment
    std::cout << "\nCycle 2: Stack increment" << std::endl;
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 3: Pull status register
    std::cout << "\nCycle 3: Pull status register" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, stack_memory[0]);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 4: Pull PCL
    std::cout << "\nCycle 4: Pull PCL" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, stack_memory[1]);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Cycle 5: Pull PCH
    std::cout << "\nCycle 5: Pull PCH" << std::endl;
    bus_state = BUS_SET_DATA(bus_state, stack_memory[2]);
    bus_state = cpu.cycle_tick(bus_state);
    
    // Check final state
    std::cout << "\n--- Final State Analysis ---" << std::endl;
    std::cout << "Final state:" << std::endl;
    std::cout << "  PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  SP=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_s()) << std::endl;
    std::cout << "  P=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_p()) << std::endl;
    
    std::cout << "\nExpected state (from ProcessorTests):" << std::endl;
    std::cout << "  PC=0x65AA" << std::endl;
    std::cout << "  P=0xAC" << std::endl;
    
    // Check if results match
    bool pc_match = (cpu.get_pc() == 0x65AA);
    bool p_match = (cpu.get_p() == 0xAC);
    
    std::cout << "\nResult comparison:" << std::endl;
    std::cout << "  PC match: " << (pc_match ? "PASS" : "FAIL") << std::endl;
    std::cout << "  P match: " << (p_match ? "PASS" : "FAIL") << std::endl;
    
    if (!pc_match) {
        std::cout << "  PC error: expected 0x65AA, got 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
        std::cout << "  PC difference: 0x" << std::hex << (cpu.get_pc() ^ 0x65AA) << std::endl;
    }
    
    if (!p_match) {
        std::cout << "  P error: expected 0xAC, got 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_p()) << std::endl;
        std::cout << "  P difference: 0x" << std::hex << (cpu.get_p() ^ 0xAC) << std::endl;
        
        // Analyze flag differences
        uint8_t expected_p = 0xAC;
        uint8_t actual_p = cpu.get_p();
        std::cout << "  Flag analysis:" << std::endl;
        std::cout << "    N: expected=" << ((expected_p & 0x80) ? 1 : 0) << ", actual=" << ((actual_p & 0x80) ? 1 : 0) << std::endl;
        std::cout << "    V: expected=" << ((expected_p & 0x40) ? 1 : 0) << ", actual=" << ((actual_p & 0x40) ? 1 : 0) << std::endl;
        std::cout << "    U: expected=" << ((expected_p & 0x20) ? 1 : 0) << ", actual=" << ((actual_p & 0x20) ? 1 : 0) << std::endl;
        std::cout << "    B: expected=" << ((expected_p & 0x10) ? 1 : 0) << ", actual=" << ((actual_p & 0x10) ? 1 : 0) << std::endl;
        std::cout << "    D: expected=" << ((expected_p & 0x08) ? 1 : 0) << ", actual=" << ((actual_p & 0x08) ? 1 : 0) << std::endl;
        std::cout << "    I: expected=" << ((expected_p & 0x04) ? 1 : 0) << ", actual=" << ((actual_p & 0x04) ? 1 : 0) << std::endl;
        std::cout << "    Z: expected=" << ((expected_p & 0x02) ? 1 : 0) << ", actual=" << ((actual_p & 0x02) ? 1 : 0) << std::endl;
        std::cout << "    C: expected=" << ((expected_p & 0x01) ? 1 : 0) << ", actual=" << ((actual_p & 0x01) ? 1 : 0) << std::endl;
    }
    
    return (pc_match && p_match) ? 0 : 1;
}