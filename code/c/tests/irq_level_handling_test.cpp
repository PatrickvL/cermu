/*
 * IRQ Level Handling and Maskable Interrupt Processing Test
 * 
 * This test validates the enhanced IRQ implementation for:
 * - Level-sensitive IRQ detection
 * - Proper IRQ masking with I flag
 * - Multiple IRQ source handling
 * - SEI/CLI instruction synchronization
 * - Hardware-accurate IRQ behavior
 */

#include <iostream>
#include <cassert>
#include <iomanip>
#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "../src/core/system_lines.h"

using namespace fam65xx_cpp;

// Test configuration - use NMOS 6502 for this test
using TestConfig = config_6502;
using TestCPU = fam65xx<TestConfig>;

// Test helper class
class IRQTestHarness {
private:
    TestCPU cpu;
    uint16_t test_memory[0x10000] = {0};
    
public:
    IRQTestHarness() {
        // Initialize test memory with simple program
        test_memory[0xFFFC] = 0x00;  // Reset vector low
        test_memory[0xFFFD] = 0x80;  // Reset vector high -> $8000
        test_memory[0xFFFE] = 0x00;  // IRQ vector low  
        test_memory[0xFFFF] = 0x90;  // IRQ vector high -> $9000
        
        // Simple test program at $8000
        test_memory[0x8000] = 0xEA;  // NOP
        test_memory[0x8001] = 0x58;  // CLI (Clear Interrupt Disable)
        test_memory[0x8002] = 0xEA;  // NOP
        test_memory[0x8003] = 0x78;  // SEI (Set Interrupt Disable)
        test_memory[0x8004] = 0xEA;  // NOP
        test_memory[0x8005] = 0x4C;  // JMP $8005 (infinite loop)
        test_memory[0x8006] = 0x05;
        test_memory[0x8007] = 0x80;
        
        // IRQ handler at $9000
        test_memory[0x9000] = 0x40;  // RTI
        
        cpu.init();
    }
    
    bus_state_t read_memory(uint16_t addr) {
        bus_state_t state = 0;
        return BUS_SET_DATA(state, test_memory[addr]);
    }
    
    void write_memory(uint16_t addr, uint8_t data) {
        test_memory[addr] = data;
    }
    
    bus_state_t tick() {
        uint16_t pc = cpu.get_pc();
        bus_state_t bus_state = read_memory(pc);
        bus_state = cpu.cycle_tick(bus_state);
        
        // Handle write operations
        if (bus_state & BUS_BIT(BUS_RW_BIT)) {
            // Read operation - data already provided
        } else {
            // Write operation - store data
            uint16_t addr = ((bus_state >> 16) & 0xFFFF);
            uint8_t data = BUS_GET_DATA(bus_state);
            write_memory(addr, data);
        }
        
        return bus_state;
    }
    
    void run_cycles(int count) {
        for (int i = 0; i < count; i++) {
            tick();
        }
    }
    
    void reset_and_start() {
        cpu.reset();
        run_cycles(8);  // Complete reset sequence
    }
    
    TestCPU& get_cpu() { return cpu; }
    
    void print_cpu_state() {
        std::cout << "PC: $" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc()
                  << " A: $" << std::setw(2) << (int)cpu.get_a()
                  << " X: $" << std::setw(2) << (int)cpu.get_x()
                  << " Y: $" << std::setw(2) << (int)cpu.get_y()
                  << " S: $" << std::setw(2) << (int)cpu.get_s()
                  << " P: $" << std::setw(2) << (int)cpu.get_p()
                  << " FLAGS: $" << std::setw(4) << cpu.get_state_flags()
                  << std::dec << std::endl;
    }
};

// Test 1: Basic IRQ level detection
void test_basic_irq_level_detection() {
    std::cout << "\n=== Test 1: Basic IRQ Level Detection ===" << std::endl;
    
    IRQTestHarness harness;
    auto& cpu = harness.get_cpu();
    
    harness.reset_and_start();
    
    // Initially I flag should be set (interrupts disabled)
    assert((cpu.get_p() & P_IRQ_DIS) != 0);
    std::cout << "✓ I flag initially set after reset" << std::endl;
    
    // IRQ pin inactive (high) - should not trigger IRQ
    cpu.irq(true);
    assert(!(cpu.get_state_flags() & STATE_IRQ_PENDING));
    std::cout << "✓ IRQ pin high does not trigger interrupt" << std::endl;
    
    // IRQ pin active (low) - should set IRQ line but not pending (masked)
    cpu.irq(false);
    assert(cpu.get_state_flags() & STATE_IRQ_LINE);
    assert(!(cpu.get_state_flags() & STATE_IRQ_PENDING));  // Masked by I flag
    std::cout << "✓ IRQ pin low sets IRQ line but doesn't trigger interrupt (masked)" << std::endl;
    
    // Clear I flag manually and verify IRQ becomes pending
    cpu.set_p(cpu.get_p() & ~P_IRQ_DIS);
    cpu.handle_interrupt_flag_change();
    assert(cpu.get_state_flags() & STATE_IRQ_PENDING);
    std::cout << "✓ Clearing I flag makes pending IRQ active" << std::endl;
    
    // IRQ pin back to high - should clear both line and pending
    cpu.irq(true);
    assert(!(cpu.get_state_flags() & STATE_IRQ_LINE));
    assert(!(cpu.get_state_flags() & STATE_IRQ_PENDING));
    std::cout << "✓ IRQ pin high clears IRQ line and pending states" << std::endl;
}

// Test 2: SEI/CLI instruction synchronization
void test_sei_cli_synchronization() {
    std::cout << "\n=== Test 2: SEI/CLI Instruction Synchronization ===" << std::endl;
    
    IRQTestHarness harness;
    auto& cpu = harness.get_cpu();
    
    harness.reset_and_start();
    
    // Run until we reach CLI instruction at $8001
    while (cpu.get_pc() != 0x8001) {
        harness.tick();
    }
    
    // Set IRQ pin active before CLI
    cpu.irq(false);
    assert(cpu.get_state_flags() & STATE_IRQ_LINE);
    assert(!(cpu.get_state_flags() & STATE_IRQ_PENDING));  // Still masked
    
    // Execute CLI instruction
    harness.run_cycles(2);  // CLI is 2 cycles
    
    // After CLI, IRQ should become pending
    assert(!(cpu.get_p() & P_IRQ_DIS));  // I flag cleared
    assert(cpu.get_state_flags() & STATE_IRQ_PENDING);  // Now pending
    std::cout << "✓ CLI instruction properly unmasks IRQ" << std::endl;
    
    // Continue until we reach SEI instruction at $8003
    while (cpu.get_pc() != 0x8003) {
        harness.tick();
    }
    
    // Execute SEI instruction
    harness.run_cycles(2);  // SEI is 2 cycles
    
    // After SEI, IRQ should be masked again
    assert(cpu.get_p() & P_IRQ_DIS);  // I flag set
    assert(!(cpu.get_state_flags() & STATE_IRQ_PENDING));  // No longer pending
    std::cout << "✓ SEI instruction properly masks IRQ" << std::endl;
}

// Test 3: Multiple IRQ source handling
void test_multiple_irq_sources() {
    std::cout << "\n=== Test 3: Multiple IRQ Source Handling ===" << std::endl;
    
    IRQTestHarness harness;
    auto& cpu = harness.get_cpu();
    
    harness.reset_and_start();
    
    // Clear I flag to enable interrupts
    cpu.set_p(cpu.get_p() & ~P_IRQ_DIS);
    cpu.handle_interrupt_flag_change();
    
    // Test individual IRQ sources
    cpu.irq_source(0, true);  // Activate source 0
    assert(cpu.get_state_flags() & STATE_IRQ_LINE);
    assert(cpu.get_state_flags() & STATE_IRQ_PENDING);
    std::cout << "✓ IRQ source 0 activation triggers interrupt" << std::endl;
    
    cpu.irq_source(1, true);  // Activate source 1 (multiple sources)
    assert(cpu.get_state_flags() & STATE_IRQ_LINE);
    assert(cpu.get_state_flags() & STATE_IRQ_PENDING);
    std::cout << "✓ Multiple IRQ sources maintain interrupt state" << std::endl;
    
    cpu.irq_source(0, false); // Deactivate source 0
    assert(cpu.get_state_flags() & STATE_IRQ_LINE);   // Still active (source 1)
    assert(cpu.get_state_flags() & STATE_IRQ_PENDING);
    std::cout << "✓ Deactivating one source maintains interrupt (other active)" << std::endl;
    
    cpu.irq_source(1, false); // Deactivate source 1 (all sources off)
    assert(!(cpu.get_state_flags() & STATE_IRQ_LINE));
    assert(!(cpu.get_state_flags() & STATE_IRQ_PENDING));
    std::cout << "✓ Deactivating all sources clears interrupt" << std::endl;
}

// Test 4: IRQ timing and priority
void test_irq_timing_priority() {
    std::cout << "\n=== Test 4: IRQ Timing and Priority ===" << std::endl;
    
    IRQTestHarness harness;
    auto& cpu = harness.get_cpu();
    
    harness.reset_and_start();
    
    // Clear I flag and set up IRQ
    cpu.set_p(cpu.get_p() & ~P_IRQ_DIS);
    cpu.irq(false);  // Activate IRQ
    
    // IRQ should be processed at instruction boundary (cycle_step == 0)
    while (cpu.get_cycle_step() != 0) {
        harness.tick();
    }
    
    // Next tick should start IRQ sequence
    harness.tick();
    assert(cpu.get_state_flags() & STATE_INTERRUPT_SEQUENCE);
    assert(cpu.get_opcode() == fam65xx_cpp::VIRTUAL_OPCODE_IRQ);
    std::cout << "✓ IRQ processed at instruction boundary" << std::endl;
    
    // Complete IRQ sequence (7 cycles)
    harness.run_cycles(6);  // Complete remaining cycles
    
    // Should be at IRQ handler now
    assert(cpu.get_pc() == 0x9000);
    assert(!(cpu.get_state_flags() & STATE_INTERRUPT_SEQUENCE));
    assert(cpu.get_p() & P_IRQ_DIS);  // I flag set during IRQ
    std::cout << "✓ IRQ sequence completed, jumped to handler" << std::endl;
}

// Test 5: Edge cases and corner conditions
void test_edge_cases() {
    std::cout << "\n=== Test 5: Edge Cases and Corner Conditions ===" << std::endl;
    
    IRQTestHarness harness;
    auto& cpu = harness.get_cpu();
    
    harness.reset_and_start();
    
    // Test IRQ state during reset
    cpu.irq(false);  // Set IRQ active
    cpu.reset();     // Reset should clear all IRQ state
    
    assert(!(cpu.get_state_flags() & STATE_IRQ_LINE));
    assert(!(cpu.get_state_flags() & STATE_IRQ_PENDING));
    std::cout << "✓ Reset clears all IRQ state" << std::endl;
    
    // Test rapid IRQ pin changes
    cpu.set_p(cpu.get_p() & ~P_IRQ_DIS);  // Enable interrupts
    
    cpu.irq(false);  // IRQ active
    cpu.irq(true);   // IRQ inactive
    cpu.irq(false);  // IRQ active again
    
    assert(cpu.get_state_flags() & STATE_IRQ_LINE);
    assert(cpu.get_state_flags() & STATE_IRQ_PENDING);
    std::cout << "✓ Rapid IRQ pin changes handled correctly" << std::endl;
    
    // Test IRQ with all sources
    for (int i = 0; i < 8; i++) {
        cpu.irq_source(i, true);
    }
    assert(cpu.get_state_flags() & STATE_IRQ_LINE);
    assert(cpu.get_state_flags() & STATE_IRQ_PENDING);
    
    for (int i = 0; i < 8; i++) {
        cpu.irq_source(i, false);
    }
    assert(!(cpu.get_state_flags() & STATE_IRQ_LINE));
    assert(!(cpu.get_state_flags() & STATE_IRQ_PENDING));
    std::cout << "✓ All 8 IRQ sources handled correctly" << std::endl;
}

int main() {
    std::cout << "IRQ Level Handling and Maskable Interrupt Processing Test Suite" << std::endl;
    std::cout << "================================================================" << std::endl;
    
    try {
        test_basic_irq_level_detection();
        test_sei_cli_synchronization();
        test_multiple_irq_sources();
        test_irq_timing_priority();
        test_edge_cases();
        
        std::cout << "\n🎉 All IRQ level handling tests passed!" << std::endl;
        std::cout << "✅ Enhanced IRQ implementation validated successfully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}