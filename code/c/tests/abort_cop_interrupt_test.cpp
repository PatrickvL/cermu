#include <iostream>
#include <cassert>
#include "../src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "../src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "../src/core/system_lines.h"

using namespace fam65xx_cpp;

// Simple test harness for 65C816 tests
template<typename Config>
class SimpleTestHarness {
public:
    uint8_t memory[65536];
    
    SimpleTestHarness() {
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0;
        }
    }
    
    void reset_bus_state() {
        // Initialize bus state if needed
    }
};

void test_abort_interrupt() {
    std::cout << "Testing ABORT interrupt (65C816)..." << std::endl;
    
    // Use 65C816 configuration with ABORT pin support
    using config = config_65c816;
    fam65xx<config> cpu;
    SimpleTestHarness<config> harness;
    
    // Initialize CPU
    cpu.init();
    cpu.set_pc(0x8000);
    cpu.set_s(0xFF);
    
    // Set up ABORT vector ($FFE8/$FFE9) to point to $9000 BEFORE clearing reset
    harness.memory[0xFFE8] = 0x00;  // Low byte
    harness.memory[0xFFE9] = 0x90;  // High byte
    
    // Set up RESET vector too so reset completes properly
    harness.memory[0xFFFC] = 0x00;  // Low byte
    harness.memory[0xFFFD] = 0x80;  // High byte -> start at $8000
    
    // Clear the RESET pending flag that was set during init by running a complete reset sequence
    // This simulates the CPU completing its reset sequence with proper memory responses
    bus_state_t init_bus_state = 0;
    init_bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY high
    
    // Run complete reset sequence (7 cycles) with proper memory responses
    for (int i = 0; i < 10; i++) {
        uint16_t address = cpu.get_address();
        bool rw = cpu.get_rw();
        
        if (rw) {
            // Read cycle - provide memory data
            uint8_t data = harness.memory[address];
            init_bus_state = BUS_SET_DATA(init_bus_state, data);
        }
        
        init_bus_state = cpu.cycle_tick(init_bus_state);
        
        // Check if reset sequence is completely done
        if ((cpu.get_state_flags() & (STATE_RESET_PENDING | STATE_INTERRUPT_SEQUENCE)) == 0) {
            std::cout << "Reset sequence completed after " << i + 1 << " cycles" << std::endl;
            break;
        }
    }
    
    // Initialize harness state
    harness.reset_bus_state();
    
    // Now trigger ABORT interrupt via pin - CPU should be ready
    cpu.abort_pin(false);  // ABORT is active-low
    
    std::cout << "State flags after ABORT trigger: 0x" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "Current opcode: " << cpu.get_opcode() << std::endl;
    
    // Execute several cycles to complete the ABORT interrupt sequence
    bus_state_t bus_state = 0;
    // Set all control pins to their inactive (high) state to prevent interference
    bus_state |= BUS_BIT(BUS_RDY_BIT);   // RDY high (ready)
    if constexpr (config::has_be_pin) {
        bus_state |= BUS_BIT(BUS_BE_BIT);    // BE high (bus enabled)
    }
    if constexpr (config::has_abort_pin) {
        bus_state |= BUS_BIT(BUS_ABORT_BIT); // ABORT high (inactive)
    }
    
    // ABORT interrupt should have 7 cycles like other interrupts
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t address = cpu.get_address();
        bool rw = cpu.get_rw();
        uint8_t data = 0;
        
        if (rw) {
            // Read cycle
            if (address == 0xFFE8 || address == 0xFFE9) {
                data = harness.memory[address];
                std::cout << "Reading ABORT vector from $" << std::hex << address 
                         << " = $" << (int)data << std::endl;
            } else {
                data = harness.memory[address];
            }
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            // Write cycle (stack operations)
            data = cpu.get_write_data();
            harness.memory[address] = data;
            std::cout << "Writing to stack $" << std::hex << address 
                     << " = $" << (int)data << std::endl;
        }
        
        // Set RDY high to prevent wait states
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "Cycle " << cycle + 1 << ": PC=$" << std::hex << cpu.get_pc()
                 << " SP=$" << (int)cpu.get_s() << " Address=$" << address
                 << " RW=" << (rw ? "R" : "W") << " Opcode=" << cpu.get_opcode()
                 << " StateFlags=0x" << cpu.get_state_flags() << std::endl;
        
        // Check if interrupt sequence is complete
        if ((cpu.get_state_flags() & STATE_INTERRUPT_SEQUENCE) == 0) {
            std::cout << "ABORT interrupt sequence completed after " << cycle + 1 << " cycles" << std::endl;
            break;
        }
    }
    
    // Verify ABORT interrupt results
    assert(cpu.get_pc() == 0x9000);  // Should jump to ABORT vector
    assert((cpu.get_p() & P_IRQ_DIS) != 0);  // I flag should be set
    assert(cpu.get_s() == 0xFC);  // Stack pointer should be decremented by 3
    
    // Verify stack contents (PCH, PCL, P)
    assert(harness.memory[0x01FF] == 0x80);  // PCH
    assert(harness.memory[0x01FE] == 0x00);  // PCL
    // P register on stack should have I flag set
    
    std::cout << "ABORT interrupt test passed!" << std::endl;
}

void test_cop_instruction() {
    std::cout << "Testing COP instruction (65C816)..." << std::endl;
    
    // Use 65C816 configuration with COP instruction support
    using config = config_65c816;
    fam65xx<config> cpu;
    SimpleTestHarness<config> harness;
    
    // Initialize CPU
    cpu.init();
    cpu.set_pc(0x8000);
    cpu.set_s(0xFF);
    
    // Set up vectors BEFORE clearing reset
    harness.memory[0xFFE4] = 0x00;  // COP vector -> $A000
    harness.memory[0xFFE5] = 0xA0;
    harness.memory[0xFFFC] = 0x00;  // RESET vector -> $8000
    harness.memory[0xFFFD] = 0x80;
    
    // Place COP instruction ($02) with signature byte
    harness.memory[0x8000] = 0x02;  // COP opcode
    harness.memory[0x8001] = 0x55;  // Signature byte
    
    // Clear the RESET pending flag that was set during init by running a complete reset sequence
    bus_state_t init_bus_state = 0;
    init_bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY high
    
    // Run complete reset sequence with proper memory responses
    for (int i = 0; i < 10; i++) {
        uint16_t address = cpu.get_address();
        bool rw = cpu.get_rw();
        
        if (rw) {
            // Read cycle - provide memory data
            uint8_t data = harness.memory[address];
            init_bus_state = BUS_SET_DATA(init_bus_state, data);
        }
        
        init_bus_state = cpu.cycle_tick(init_bus_state);
        
        // Check if reset sequence is completely done
        if ((cpu.get_state_flags() & (STATE_RESET_PENDING | STATE_INTERRUPT_SEQUENCE)) == 0) {
            std::cout << "Reset sequence completed after " << i + 1 << " cycles" << std::endl;
            break;
        }
    }
    
    // Initialize harness state
    harness.reset_bus_state();
    
    // Execute several cycles to complete the COP instruction sequence
    bus_state_t bus_state = 0;
    // Set all control pins to their inactive (high) state to prevent interference
    bus_state |= BUS_BIT(BUS_RDY_BIT);   // RDY high (ready)
    if constexpr (config::has_be_pin) {
        bus_state |= BUS_BIT(BUS_BE_BIT);    // BE high (bus enabled)
    }
    if constexpr (config::has_abort_pin) {
        bus_state |= BUS_BIT(BUS_ABORT_BIT); // ABORT high (inactive)
    }
    
    // COP instruction should have 7 cycles like BRK
    for (int cycle = 0; cycle < 10; cycle++) {
        uint16_t address = cpu.get_address();
        bool rw = cpu.get_rw();
        uint8_t data = 0;
        
        if (rw) {
            // Read cycle
            if (address == 0xFFE4 || address == 0xFFE5) {
                data = harness.memory[address];
                std::cout << "Reading COP vector from $" << std::hex << address 
                         << " = $" << (int)data << std::endl;
            } else {
                data = harness.memory[address];
            }
            bus_state = BUS_SET_DATA(bus_state, data);
        } else {
            // Write cycle (stack operations)
            data = cpu.get_write_data();
            harness.memory[address] = data;
            std::cout << "Writing to stack $" << std::hex << address 
                     << " = $" << (int)data << std::endl;
        }
        
        // Set RDY high to prevent wait states
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        bus_state = cpu.cycle_tick(bus_state);
        
        std::cout << "Cycle " << cycle + 1 << ": PC=$" << std::hex << cpu.get_pc() 
                 << " SP=$" << (int)cpu.get_s() << " Address=$" << address 
                 << " RW=" << (rw ? "R" : "W") << std::endl;
        
        // Check if interrupt sequence is complete
        if ((cpu.get_state_flags() & STATE_INTERRUPT_SEQUENCE) == 0) {
            std::cout << "COP instruction sequence completed after " << cycle + 1 << " cycles" << std::endl;
            break;
        }
    }
    
    // Verify COP instruction results
    assert(cpu.get_pc() == 0xA000);  // Should jump to COP vector
    assert((cpu.get_p() & P_IRQ_DIS) != 0);  // I flag should be set
    assert(cpu.get_s() == 0xFC);  // Stack pointer should be decremented by 3
    
    // Verify stack contents (PCH of return address, PCL of return address, P)
    // COP should push PC+2 as return address (after signature byte)
    assert(harness.memory[0x01FF] == 0x80);  // PCH of return address
    assert(harness.memory[0x01FE] == 0x02);  // PCL of return address (PC+2)
    // P register on stack should have I flag set
    
    std::cout << "COP instruction test passed!" << std::endl;
}

void test_interrupt_priority() {
    std::cout << "Testing interrupt priority (ABORT > NMI > COP > IRQ)..." << std::endl;
    
    // Use 65C816 configuration
    using config = config_65c816;
    fam65xx<config> cpu;
    SimpleTestHarness<config> harness;
    
    // Initialize CPU
    cpu.init();
    cpu.set_pc(0x8000);
    cpu.set_s(0xFF);
    
    // Set up interrupt vectors BEFORE clearing reset
    harness.memory[0xFFE8] = 0x00;  // ABORT vector -> $9000
    harness.memory[0xFFE9] = 0x90;
    harness.memory[0xFFFA] = 0x00;  // NMI vector -> $A000
    harness.memory[0xFFFB] = 0xA0;
    harness.memory[0xFFE4] = 0x00;  // COP vector -> $B000
    harness.memory[0xFFE5] = 0xB0;
    harness.memory[0xFFFE] = 0x00;  // IRQ vector -> $C000
    harness.memory[0xFFFF] = 0xC0;
    harness.memory[0xFFFC] = 0x00;  // RESET vector -> $8000
    harness.memory[0xFFFD] = 0x80;
    
    // Clear the RESET pending flag that was set during init by running a complete reset sequence
    bus_state_t init_bus_state = 0;
    init_bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY high
    
    // Run complete reset sequence with proper memory responses
    for (int i = 0; i < 10; i++) {
        uint16_t address = cpu.get_address();
        bool rw = cpu.get_rw();
        
        if (rw) {
            // Read cycle - provide memory data
            uint8_t data = harness.memory[address];
            init_bus_state = BUS_SET_DATA(init_bus_state, data);
        }
        
        init_bus_state = cpu.cycle_tick(init_bus_state);
        
        // Check if reset sequence is completely done
        if ((cpu.get_state_flags() & (STATE_RESET_PENDING | STATE_INTERRUPT_SEQUENCE)) == 0) {
            std::cout << "Reset sequence completed after " << i + 1 << " cycles" << std::endl;
            break;
        }
    }
    
    // Initialize harness state
    harness.reset_bus_state();
    
    // Debug: Check if ABORT pin support is working
    std::cout << "Before ABORT pin: state flags = 0x" << std::hex << cpu.get_state_flags() << std::endl;
    std::cout << "STATE_ABORT_PENDING constant = 0x" << std::hex << STATE_ABORT_PENDING << std::endl;
    std::cout << "STATE_DMA_CYCLE constant = 0x" << std::hex << STATE_DMA_CYCLE << std::endl;
    
    // Trigger multiple interrupts simultaneously
    cpu.abort_pin(false);  // ABORT active
    std::cout << "After ABORT pin: state flags = 0x" << std::hex << cpu.get_state_flags() << std::endl;
    
    cpu.nmi_pin(false);    // NMI active
    std::cout << "After NMI pin: state flags = 0x" << std::hex << cpu.get_state_flags() << std::endl;
    
    cpu.cop_instruction(); // COP pending
    std::cout << "After COP instruction: state flags = 0x" << std::hex << cpu.get_state_flags() << std::endl;
    
    cpu.irq_pin(false);    // IRQ active
    std::cout << "After IRQ pin: state flags = 0x" << std::hex << cpu.get_state_flags() << std::endl;
    
    // Execute one cycle - ABORT should win due to highest priority
    bus_state_t bus_state = 0;
    // Set control pins to their inactive state but DON'T manipulate ABORT pin
    // as we want to preserve the edge detection that was triggered by abort_pin(false)
    bus_state |= BUS_BIT(BUS_RDY_BIT);   // RDY high (ready)
    if constexpr (config::has_be_pin) {
        bus_state |= BUS_BIT(BUS_BE_BIT);    // BE high (bus enabled)
    }
    // NOTE: Deliberately NOT setting BUS_ABORT_BIT to avoid interfering with edge detection
    
    // First cycle should start ABORT sequence
    bus_state = cpu.cycle_tick(bus_state);
    
    // Debug output
    std::cout << "After cycle_tick: opcode=" << cpu.get_opcode()
              << " (expected=" << VIRTUAL_OPCODE_ABORT << ")" << std::endl;
    std::cout << "State flags: 0x" << std::hex << cpu.get_state_flags() << std::endl;
    
    // Check that ABORT interrupt is being serviced
    assert((cpu.get_state_flags() & STATE_INTERRUPT_SEQUENCE) != 0);
    assert(cpu.get_opcode() == VIRTUAL_OPCODE_ABORT);
    
    std::cout << "Interrupt priority test passed - ABORT has highest priority!" << std::endl;
}

void test_simple_abort_flag() {
    std::cout << "Testing simple ABORT flag setting..." << std::endl;
    
    using config = config_65c816;
    fam65xx<config> cpu;
    
    cpu.init();
    std::cout << "After init: flags = 0x" << std::hex << cpu.get_state_flags() << std::endl;
    
    // Simple test: just call abort_pin
    cpu.abort_pin(false);
    std::cout << "After abort_pin(false): flags = 0x" << std::hex << cpu.get_state_flags() << std::endl;
    
    // Check if ABORT flag is set
    if (cpu.get_state_flags() & STATE_ABORT_PENDING) {
        std::cout << "ABORT flag is correctly set!" << std::endl;
    } else {
        std::cout << "ERROR: ABORT flag is NOT set!" << std::endl;
    }
}

int main() {
    try {
        test_simple_abort_flag();
        test_abort_interrupt();
        test_cop_instruction();
        test_interrupt_priority();
        
        std::cout << "\nAll ABORT/COP interrupt tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}