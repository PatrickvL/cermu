// NMI Edge Detection Test - Validate hardware-accurate NMI implementation
// Tests proper falling edge detection and interrupt timing synchronization

#include <iostream>
#include <cassert>
#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "../src/core/system_lines.h"

using namespace fam65xx_cpp;

// Test configuration for basic 6502 NMOS
using TestConfig = config_6502;

void test_nmi_edge_detection() {
    std::cout << "Testing NMI edge detection..." << std::endl;
    
    fam65xx<TestConfig> cpu;
    cpu.init();
    
    // Initial state: NMI pin should be high (inactive)
    // Verify no NMI is pending initially
    assert(!(cpu.get_state_flags() & STATE_NMI_PENDING));
    assert(!(cpu.get_state_flags() & STATE_NMI_EDGE));
    
    // Test 1: NMI pin starts high, stays high - no interrupt should occur
    cpu.nmi_pin(true);  // High (inactive)
    cpu.nmi_pin(true);  // Still high
    assert(!(cpu.get_state_flags() & STATE_NMI_PENDING));
    assert(!(cpu.get_state_flags() & STATE_NMI_EDGE));
    std::cout << "✓ NMI pin high-to-high: No interrupt triggered" << std::endl;
    
    // Test 2: NMI pin goes from high to low (falling edge) - should trigger interrupt
    cpu.nmi_pin(false); // Low (active) - falling edge detected
    assert(cpu.get_state_flags() & STATE_NMI_PENDING);
    assert(cpu.get_state_flags() & STATE_NMI_EDGE);
    std::cout << "✓ NMI pin high-to-low: Interrupt triggered" << std::endl;
    
    // Clear states for next test
    cpu.clear_state(STATE_NMI_PENDING | STATE_NMI_EDGE);
    
    // Test 3: NMI pin stays low - no additional interrupt should occur
    cpu.nmi_pin(false); // Still low
    cpu.nmi_pin(false); // Still low
    assert(!(cpu.get_state_flags() & STATE_NMI_PENDING));
    assert(!(cpu.get_state_flags() & STATE_NMI_EDGE));
    std::cout << "✓ NMI pin low-to-low: No additional interrupt" << std::endl;
    
    // Test 4: NMI pin goes from low to high (rising edge) - no interrupt should occur
    cpu.nmi_pin(true);  // High (inactive) - rising edge, should not trigger
    assert(!(cpu.get_state_flags() & STATE_NMI_PENDING));
    assert(!(cpu.get_state_flags() & STATE_NMI_EDGE));
    std::cout << "✓ NMI pin low-to-high: No interrupt triggered" << std::endl;
    
    // Test 5: Another falling edge after rising edge - should trigger interrupt again
    cpu.nmi_pin(false); // Low (active) - another falling edge
    assert(cpu.get_state_flags() & STATE_NMI_PENDING);
    assert(cpu.get_state_flags() & STATE_NMI_EDGE);
    std::cout << "✓ NMI pin second falling edge: Interrupt triggered again" << std::endl;
    
    // Test 6: Legacy NMI method still works
    cpu.clear_state(STATE_NMI_PENDING | STATE_NMI_EDGE);
    cpu.nmi(); // Software-triggered NMI
    assert(cpu.get_state_flags() & STATE_NMI_PENDING);
    assert(cpu.get_state_flags() & STATE_NMI_EDGE);
    std::cout << "✓ Legacy nmi() method: Works correctly" << std::endl;
    
    // Test 7: Reset clears NMI pin state and pending interrupts
    cpu.reset();
    assert(cpu.get_state_flags() & STATE_RESET_PENDING);
    assert(!(cpu.get_state_flags() & STATE_NMI_PENDING));
    assert(!(cpu.get_state_flags() & STATE_NMI_EDGE));
    std::cout << "✓ Reset: Clears NMI states and resets pin state" << std::endl;
    
    std::cout << "All NMI edge detection tests passed!" << std::endl;
}

void test_nmi_timing_synchronization() {
    std::cout << "\nTesting NMI timing synchronization..." << std::endl;
    
    fam65xx<TestConfig> cpu;
    cpu.init();
    
    // Clear reset state and set up for normal execution
    cpu.clear_state(STATE_RESET_PENDING);
    
    // Set up a simple program: NOP instruction at $0000
    bus_state_t bus_state = 0;
    bus_state = BUS_SET_DATA(bus_state, 0xEA); // NOP opcode
    bus_state |= BUS_BIT(BUS_RDY_BIT); // Set RDY high (ready)
    
    // First cycle: Fetch NOP opcode (cycle_step becomes 1)
    bus_state = cpu.cycle_tick(bus_state);
    
    // Debug: Print current state
    std::cout << "After first cycle - cycle_step: " << (int)cpu.get_cycle_step()
              << ", opcode: 0x" << std::hex << cpu.get_opcode() << std::dec
              << ", state_flags: 0x" << std::hex << cpu.get_state_flags() << std::dec << std::endl;
    
    // Trigger NMI edge during instruction execution
    cpu.nmi_pin(false); // Falling edge - should latch NMI
    assert(cpu.get_state_flags() & STATE_NMI_PENDING);
    assert(cpu.get_state_flags() & STATE_NMI_EDGE);
    
    // Second cycle: Complete NOP instruction - NMI should not interrupt mid-instruction
    bus_state = cpu.cycle_tick(bus_state);
    
    // Debug: Print state after second cycle
    std::cout << "After second cycle - cycle_step: " << (int)cpu.get_cycle_step()
              << ", opcode: 0x" << std::hex << cpu.get_opcode() << std::dec
              << ", state_flags: 0x" << std::hex << cpu.get_state_flags() << std::dec << std::endl;
    
    // After NOP completes, should be at instruction boundary
    assert(cpu.get_cycle_step() == 0); // Should be at instruction boundary
    
    // Next cycle should start NMI interrupt sequence
    bus_state = cpu.cycle_tick(bus_state); // Should start NMI interrupt
    
    // Debug: Print state after third cycle
    std::cout << "After third cycle - cycle_step: " << (int)cpu.get_cycle_step()
              << ", opcode: 0x" << std::hex << cpu.get_opcode() << std::dec
              << ", interrupt_seq: " << cpu.get_state(STATE_INTERRUPT_SEQUENCE) << std::endl;
    
    assert(cpu.get_state(STATE_INTERRUPT_SEQUENCE));
    assert(cpu.get_opcode() == fam65xx_cpp::VIRTUAL_OPCODE_NMI);
    assert(!(cpu.get_state_flags() & STATE_NMI_EDGE)); // Should be cleared when servicing
    
    std::cout << "✓ NMI timing: Interrupt waits for instruction boundary" << std::endl;
    std::cout << "All NMI timing tests passed!" << std::endl;
}

int main() {
    try {
        test_nmi_edge_detection();
        test_nmi_timing_synchronization();
        
        std::cout << "\n🎉 All NMI edge detection and timing tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}