/**
 * @file brk_flag_setting_test.cpp
 * @brief Test BRK instruction proper B flag setting behavior
 *
 * This test validates that the BRK instruction correctly sets both the B (break) flag
 * and I (interrupt disable) flag when pushing the processor status to the stack.
 */

#include <iostream>
#include <iomanip>
#include <cassert>
#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "../src/core/system_lines.h"

using namespace fam65xx_cpp;

// Test configuration for basic 6502 NMOS
using TestConfig = config_6502;

// Memory interface for testing
class TestMemory {
private:
    uint8_t memory[65536];
    
public:
    TestMemory() {
        // Initialize memory to zero
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

/**
 * Test BRK instruction B flag setting behavior
 * 
 * The BRK instruction should:
 * 1. Set both B flag (bit 4) and I flag (bit 2) in the processor status
 * 2. Push the status with B flag set to the stack
 * 3. Jump to the IRQ vector ($FFFE/$FFFF)
 * 4. Take exactly 7 cycles
 */
void test_brk_flag_setting() {
    std::cout << "\n=== Testing BRK Flag Setting Behavior ===" << std::endl;
    
    fam65xx<TestConfig> cpu;
    TestMemory memory;
    
    cpu.init();
    cpu.clear_state(STATE_RESET_PENDING);
    
    // Set up IRQ vector
    memory.write(0xFFFE, 0x00);  // IRQ vector low byte
    memory.write(0xFFFF, 0x80);  // IRQ vector high byte
    
    // Set up test program
    memory.write(0x0200, 0x00);  // BRK instruction
    memory.write(0x0201, 0x42);  // Signature byte (should be ignored)
    
    // Set initial CPU state via bus interface
    bus_state_t bus_state = 0;
    
    // Initialize PC to $0200 and SP to $FF
    // Set processor status to clear B and I flags
    cpu.set_pc(0x0200);
    cpu.set_sp(0xFF);
    cpu.set_status(0x24);  // Clear B and I flags, set bit 5 (always 1)
    
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') 
              << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.get_sp() << std::endl;
    std::cout << "  P:  $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.get_status() << " (B=" 
              << ((cpu.get_status() & 0x10) ? "1" : "0") 
              << ", I=" << ((cpu.get_status() & 0x04) ? "1" : "0") << ")" << std::endl;
    
    // Execute BRK instruction cycle by cycle
    uint64_t initial_cycles = cpu.get_cycle_count();
    
    // BRK takes 7 cycles total
    for (int cycle = 0; cycle < 7; cycle++) {
        // Get current address from CPU
        uint16_t addr = cpu.get_address();
        
        // Simulate memory read/write based on RW line
        if (cpu.get_rw()) {
            // Read operation
            uint8_t data = memory.read(addr);
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            // Write operation
            uint8_t data = BUS_GET_DATA(bus_state);
            memory.write(addr, data);
        }
        
        bus_state |= BUS_BIT(BUS_RDY_BIT); // Set RDY high (ready)
        
        // Execute one CPU cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "  Cycle " << (cycle + 1) << ": addr=$" << std::hex << std::setw(4) << std::setfill('0') 
                  << addr << ", " << (cpu.get_rw() ? "R" : "W") 
                  << ", data=$" << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)BUS_GET_DATA(bus_state) << std::endl;
    }
    
    uint64_t cycles_taken = cpu.get_cycle_count() - initial_cycles;
    
    std::cout << "\nAfter BRK execution:" << std::endl;
    std::cout << "  Cycles taken: " << cycles_taken << std::endl;
    std::cout << "  PC: $" << std::hex << std::setw(4) << std::setfill('0') 
              << cpu.get_pc() << std::endl;
    std::cout << "  SP: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.get_sp() << std::endl;
    std::cout << "  P:  $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)cpu.get_status() << " (B=" 
              << ((cpu.get_status() & 0x10) ? "1" : "0") 
              << ", I=" << ((cpu.get_status() & 0x04) ? "1" : "0") << ")" << std::endl;
    
    // Check stack contents
    uint8_t stack_pch = memory.read(0x01FF);  // PCH pushed first
    uint8_t stack_pcl = memory.read(0x01FE);  // PCL pushed second
    uint8_t stack_p = memory.read(0x01FD);    // P pushed third
    
    std::cout << "\nStack contents:" << std::endl;
    std::cout << "  $01FF (PCH): $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)stack_pch << std::endl;
    std::cout << "  $01FE (PCL): $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)stack_pcl << std::endl;
    std::cout << "  $01FD (P):   $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)stack_p << " (B=" << ((stack_p & 0x10) ? "1" : "0") 
              << ", I=" << ((stack_p & 0x04) ? "1" : "0") << ")" << std::endl;
    
    // Validate results
    bool success = true;
    
    // Check cycle count (BRK takes exactly 7 cycles)
    if (cycles_taken != 7) {
        std::cout << "ERROR: BRK took " << cycles_taken << " cycles, expected 7" << std::endl;
        success = false;
    }
    
    // Check PC jumped to IRQ vector
    if (cpu.get_pc() != 0x8000) {
        std::cout << "ERROR: PC is $" << std::hex << cpu.get_pc() 
                  << ", expected $8000 (IRQ vector)" << std::endl;
        success = false;
    }
    
    // Check stack pointer decremented by 3
    if (cpu.get_sp() != 0xFC) {
        std::cout << "ERROR: SP is $" << std::hex << (int)cpu.get_sp() 
                  << ", expected $FC" << std::endl;
        success = false;
    }
    
    // Check I flag is set in current processor status
    if (!(cpu.get_status() & 0x04)) {
        std::cout << "ERROR: I flag not set in current processor status" << std::endl;
        success = false;
    }
    
    // Check B flag is set in current processor status
    if (!(cpu.get_status() & 0x10)) {
        std::cout << "ERROR: B flag not set in current processor status" << std::endl;
        success = false;
    }
    
    // Check return address on stack (should be PC+2 = $0202)
    uint16_t return_addr = (stack_pch << 8) | stack_pcl;
    if (return_addr != 0x0202) {
        std::cout << "ERROR: Return address on stack is $" << std::hex << return_addr 
                  << ", expected $0202" << std::endl;
        success = false;
    }
    
    // Check B flag set in pushed processor status
    if (!(stack_p & 0x10)) {
        std::cout << "ERROR: B flag not set in pushed processor status" << std::endl;
        success = false;
    }
    
    // Check I flag set in pushed processor status
    if (!(stack_p & 0x04)) {
        std::cout << "ERROR: I flag not set in pushed processor status" << std::endl;
        success = false;
    }
    
    if (success) {
        std::cout << "\n✓ BRK flag setting test PASSED" << std::endl;
        std::cout << "  - Correct cycle count (7 cycles)" << std::endl;
        std::cout << "  - B flag properly set in pushed status" << std::endl;
        std::cout << "  - I flag properly set in pushed status" << std::endl;
        std::cout << "  - Correct jump to IRQ vector" << std::endl;
        std::cout << "  - Proper stack operations" << std::endl;
    } else {
        std::cout << "\n✗ BRK flag setting test FAILED" << std::endl;
    }
    
    assert(success);
}

/**
 * Test BRK vs hardware IRQ flag differences
 * 
 * Verify that BRK sets B flag while hardware IRQ does not
 */
void test_brk_vs_irq_flag_difference() {
    std::cout << "\n=== Testing BRK vs IRQ Flag Differences ===" << std::endl;
    
    fam65xx<TestConfig> cpu;
    TestMemory memory;
    
    // Test 1: BRK instruction
    std::cout << "\nTest 1: BRK instruction" << std::endl;
    
    cpu.init();
    cpu.clear_state(STATE_RESET_PENDING);
    
    // Set up IRQ vector
    memory.write(0xFFFE, 0x00);  // IRQ vector low byte
    memory.write(0xFFFF, 0x90);  // IRQ vector high byte
    
    memory.write(0x0200, 0x00);  // BRK instruction
    cpu.set_pc(0x0200);
    cpu.set_sp(0xFF);
    cpu.set_status(0x20);  // Clear B and I flags
    
    // Execute BRK
    bus_state_t bus_state = 0;
    for (int cycle = 0; cycle < 7; cycle++) {
        uint16_t addr = cpu.get_address();
        
        if (cpu.get_rw()) {
            uint8_t data = memory.read(addr);
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            uint8_t data = BUS_GET_DATA(bus_state);
            memory.write(addr, data);
        }
        
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        bus_state = cpu.cycle_tick(bus_state);
    }
    
    uint8_t brk_pushed_status = memory.read(0x01FD);
    std::cout << "  BRK pushed status: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)brk_pushed_status << " (B=" << ((brk_pushed_status & 0x10) ? "1" : "0") << ")" << std::endl;
    
    // Test 2: Hardware IRQ
    std::cout << "\nTest 2: Hardware IRQ" << std::endl;
    
    // Reset for IRQ test
    cpu.init();
    cpu.clear_state(STATE_RESET_PENDING);
    
    memory.write(0xFFFE, 0x00);  // IRQ vector low byte
    memory.write(0xFFFF, 0x90);  // IRQ vector high byte
    
    // Set up a simple program and trigger IRQ
    memory.write(0x0200, 0xEA);  // NOP instruction
    cpu.set_pc(0x0200);
    cpu.set_sp(0xFF);
    cpu.set_status(0x20);  // Clear I flag to allow IRQ
    
    // Trigger hardware IRQ
    cpu.irq_pin(false);  // Assert IRQ line
    
    // Execute until IRQ is serviced
    bus_state = 0;
    for (int i = 0; i < 10; i++) {
        uint16_t addr = cpu.get_address();
        
        if (cpu.get_rw()) {
            uint8_t data = memory.read(addr);
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            uint8_t data = BUS_GET_DATA(bus_state);
            memory.write(addr, data);
        }
        
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        bus_state = cpu.cycle_tick(bus_state);
        
        if (cpu.get_pc() == 0x9000) break;  // IRQ vector reached
    }
    
    // Find the pushed status on stack (should be at $01FD after IRQ)
    uint8_t irq_pushed_status = memory.read(0x01FD);
    
    std::cout << "  IRQ pushed status: $" << std::hex << std::setw(2) << std::setfill('0') 
              << (int)irq_pushed_status << " (B=" << ((irq_pushed_status & 0x10) ? "1" : "0") << ")" << std::endl;
    
    // Validate the difference
    bool success = true;
    
    if (!(brk_pushed_status & 0x10)) {
        std::cout << "ERROR: BRK should set B flag in pushed status" << std::endl;
        success = false;
    }
    
    if (irq_pushed_status & 0x10) {
        std::cout << "ERROR: Hardware IRQ should NOT set B flag in pushed status" << std::endl;
        success = false;
    }
    
    if (success) {
        std::cout << "\n✓ BRK vs IRQ flag difference test PASSED" << std::endl;
        std::cout << "  - BRK correctly sets B flag" << std::endl;
        std::cout << "  - Hardware IRQ correctly leaves B flag clear" << std::endl;
    } else {
        std::cout << "\n✗ BRK vs IRQ flag difference test FAILED" << std::endl;
    }
    
    assert(success);
}

int main() {
    try {
        std::cout << "6502/65C02 BRK Flag Setting Test Suite" << std::endl;
        std::cout << "=======================================" << std::endl;
        
        test_brk_flag_setting();
        test_brk_vs_irq_flag_difference();
        
        std::cout << "\n🎉 All BRK flag setting tests completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}